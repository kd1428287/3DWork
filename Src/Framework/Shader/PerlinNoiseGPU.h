#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

#include "Application/Core/Math/PerlinNoise.h"

// PerlinNoiseの順列テーブルをGPUへ渡す。シーンに1つだけ持ち、起動時にInit()する。
class PerlinNoiseGPU {
public:
	bool Init(ID3D11Device* device, const PerlinNoise& noise = PerlinNoise::Shared()) {
		std::array<uint32_t, 512> data{};
		const auto& table = noise.GetTable();
		for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint32_t>(table[i]);

		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = static_cast<UINT>(sizeof(data));
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(uint32_t);

		D3D11_SUBRESOURCE_DATA init{};
		init.pSysMem = data.data();

		if (FAILED(device->CreateBuffer(&desc, &init, buffer_.ReleaseAndGetAddressOf()))) return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(data.size());

		return SUCCEEDED(device->CreateShaderResourceView(buffer_.Get(), &srvDesc, srv_.ReleaseAndGetAddressOf()));
	}

	// slotはPerlinNoise.hlsliのPERLIN_PERM_SLOTの番号と合わせる
	void BindCS(ID3D11DeviceContext* ctx, UINT slot) const {
		ID3D11ShaderResourceView* srv = srv_.Get();
		ctx->CSSetShaderResources(slot, 1, &srv);
	}
	void BindPS(ID3D11DeviceContext* ctx, UINT slot) const {
		ID3D11ShaderResourceView* srv = srv_.Get();
		ctx->PSSetShaderResources(slot, 1, &srv);
	}

	// デバイス解放前にKdShaderManager::Release()から呼ぶ
	void Release() {
		srv_.Reset();
		buffer_.Reset();
	}

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
};