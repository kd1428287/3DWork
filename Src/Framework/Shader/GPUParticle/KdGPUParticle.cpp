#include "Framework/KdFramework.h"

#include "KdGPUParticle.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// バッファ・シェーダーの生成、パーティクルの初期化
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool KdGPUParticle::Init(UINT maxParticleNum)
{
	Release();

	m_maxParticleNum = maxParticleNum;

	if (!CreateBuffers(maxParticleNum)) { return false; }
	if (!CreateShaders()) { return false; }

	m_cb0_Init.Create();
	m_cb0_Emit.Create();
	m_cb0_Update.Create();

	// 全パーティクルを「死亡」状態に初期化(コンピュートシェーダーで実行)
	{
		m_cb0_Init.Work().MaxParticleNum = maxParticleNum;
		m_cb0_Init.Write();

		ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

		DevCon->CSSetShader(m_CS_Init, nullptr, 0);
		DevCon->CSSetConstantBuffers(0, 1, m_cb0_Init.GetAddress());
		DevCon->CSSetUnorderedAccessViews(0, 1, &m_particleUAV, nullptr);

		UINT threadGroupNum = (maxParticleNum + 255) / 256;
		DevCon->Dispatch(threadGroupNum, 1, 1);

		// UAVバインド解除
		ID3D11UnorderedAccessView* pNullUAV = nullptr;
		DevCon->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
		DevCon->CSSetShader(nullptr, nullptr, 0);
	}

	m_initialized = true;

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// バッファ(パーティクル本体・発生カウンタ)の生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool KdGPUParticle::CreateBuffers(UINT maxParticleNum)
{
	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	//------------------------------------------
	// パーティクル本体バッファ
	//------------------------------------------
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(Particle) * maxParticleNum;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(Particle);

		if (FAILED(Dev->CreateBuffer(&desc, nullptr, &m_particleBuffer)))
		{
			assert(0 && "GPUパーティクル：本体バッファ作成失敗");
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = maxParticleNum;

		if (FAILED(Dev->CreateUnorderedAccessView(m_particleBuffer, &uavDesc, &m_particleUAV)))
		{
			assert(0 && "GPUパーティクル：本体UAV作成失敗");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = maxParticleNum;

		if (FAILED(Dev->CreateShaderResourceView(m_particleBuffer, &srvDesc, &m_particleSRV)))
		{
			assert(0 && "GPUパーティクル：本体SRV作成失敗");
			return false;
		}
	}

	//------------------------------------------
	// 発生用リングバッファの書き込みカーソル(要素数1)
	//------------------------------------------
	{
		UINT initialValue = 0;

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(UINT);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(UINT);

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = &initialValue;

		if (FAILED(Dev->CreateBuffer(&desc, &initData, &m_emitCounterBuffer)))
		{
			assert(0 && "GPUパーティクル：発生カウンタバッファ作成失敗");
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 1;

		if (FAILED(Dev->CreateUnorderedAccessView(m_emitCounterBuffer, &uavDesc, &m_emitCounterUAV)))
		{
			assert(0 && "GPUパーティクル：発生カウンタUAV作成失敗");
			return false;
		}
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンピュートシェーダー・描画用シェーダーの生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool KdGPUParticle::CreateShaders()
{
	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	// コンピュートシェーダー：初期化
	{
#include "KdGPUParticle_CS_Init.shaderInc"

		if (FAILED(Dev->CreateComputeShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_CS_Init)))
		{
			assert(0 && "GPUパーティクル：コンピュートシェーダー作成失敗(Init)");
			return false;
		}
	}

	// コンピュートシェーダー：発生
	{
#include "KdGPUParticle_CS_Emit.shaderInc"

		if (FAILED(Dev->CreateComputeShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_CS_Emit)))
		{
			assert(0 && "GPUパーティクル：コンピュートシェーダー作成失敗(Emit)");
			return false;
		}
	}

	// コンピュートシェーダー：更新
	{
#include "KdGPUParticle_CS_Update.shaderInc"

		if (FAILED(Dev->CreateComputeShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_CS_Update)))
		{
			assert(0 && "GPUパーティクル：コンピュートシェーダー作成失敗(Update)");
			return false;
		}
	}

	// 頂点シェーダー(頂点バッファ未使用のためInputLayoutは作成不要)
	{
#include "KdGPUParticle_VS.shaderInc"

		if (FAILED(Dev->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "GPUパーティクル：頂点シェーダー作成失敗");
			return false;
		}
	}

	// ピクセルシェーダー
	{
#include "KdGPUParticle_PS.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS)))
		{
			assert(0 && "GPUパーティクル：ピクセルシェーダー作成失敗");
			return false;
		}
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdGPUParticle::Release()
{
	KdSafeRelease(m_CS_Init);
	KdSafeRelease(m_CS_Emit);
	KdSafeRelease(m_CS_Update);
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS);

	KdSafeRelease(m_particleUAV);
	KdSafeRelease(m_particleSRV);
	KdSafeRelease(m_particleBuffer);

	KdSafeRelease(m_emitCounterUAV);
	KdSafeRelease(m_emitCounterBuffer);

	m_cb0_Init.Release();
	m_cb0_Emit.Release();
	m_cb0_Update.Release();

	m_initialized = false;
	m_maxParticleNum = 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// パーティクルの発生：リングバッファへcount個ぶん書き込む
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdGPUParticle::Emit(const EmitParameter& param, UINT count)
{
	if (!m_initialized || count == 0) { return; }

	cbEmit& emit = m_cb0_Emit.Work();
	emit.EmitPos = param.Position;
	emit.EmitCount = (int)count;
	emit.EmitVelocityMin = param.VelocityMin;
	emit.EmitVelocityMax = param.VelocityMax;
	emit.EmitSizeMin = param.SizeMin;
	emit.EmitSizeMax = param.SizeMax;
	emit.EmitLifeMin = param.LifeMin;
	emit.EmitLifeMax = param.LifeMax;
	emit.EmitColorStartMin = param.ColorStartMin;
	emit.EmitColorStartMax = param.ColorStartMax;
	emit.EmitColorMin = param.ColorMin;
	emit.EmitColorMax = param.ColorMax;
	emit.MaxParticleNum = m_maxParticleNum;
	emit.RandomSeed = static_cast<float>(rand() % 100000);

	m_cb0_Emit.Write();

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	DevCon->CSSetShader(m_CS_Emit, nullptr, 0);
	DevCon->CSSetConstantBuffers(0, 1, m_cb0_Emit.GetAddress());

	ID3D11UnorderedAccessView* uavs[2] = { m_particleUAV, m_emitCounterUAV };
	DevCon->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

	UINT threadGroupNum = (count + 255) / 256;
	DevCon->Dispatch(threadGroupNum, 1, 1);

	ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
	DevCon->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
	DevCon->CSSetShader(nullptr, nullptr, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 1フレームぶんのシミュレーション更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdGPUParticle::Update(float deltaTime, const Math::Vector3& gravity)
{
	if (!m_initialized) { return; }

	cbUpdate& update = m_cb0_Update.Work();
	update.MaxParticleNum = m_maxParticleNum;
	update.DeltaTime = deltaTime;
	update.Gravity = gravity;

	m_cb0_Update.Write();

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	DevCon->CSSetShader(m_CS_Update, nullptr, 0);
	DevCon->CSSetConstantBuffers(0, 1, m_cb0_Update.GetAddress());
	DevCon->CSSetUnorderedAccessViews(0, 1, &m_particleUAV, nullptr);

	UINT threadGroupNum = (m_maxParticleNum + 255) / 256;
	DevCon->Dispatch(threadGroupNum, 1, 1);

	ID3D11UnorderedAccessView* pNullUAV = nullptr;
	DevCon->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
	DevCon->CSSetShader(nullptr, nullptr, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：加算合成のビルボードとしてインスタンシング描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdGPUParticle::Draw(const std::shared_ptr<KdTexture>& texture)
{
	if (!m_initialized) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	// シェーダーのセット(頂点バッファを使わないためInputLayoutは不要＝nullptrでOK)
	shaderMgr.SetVertexShader(m_VS);
	DevCon->IASetInputLayout(nullptr);
	shaderMgr.SetPixelShader(m_PS);

	// パーティクル本体バッファをVS用SRVとしてセット
	DevCon->VSSetShaderResources(0, 1, &m_particleSRV);

	// パーティクル用テクスチャをPS用SRVとしてセット
	if (texture)
	{
		DevCon->PSSetShaderResources(1, 1, texture->WorkSRViewAddress());
	}

	// 通常テクスチャ用サンプラーをセット
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp, 0);

	// 加算合成・Z書き込み無効(重なった時に不透明に潰れないように)
	shaderMgr.ChangeBlendState(KdBlendState::Add);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	DevCon->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

	// パーティクル1粒＝4頂点(板ポリ) × 最大パーティクル数ぶんをインスタンシング描画
	// (死亡中のパーティクルはVS側でサイズ0にして実質見えなくしている)
	DevCon->DrawInstanced(4, m_maxParticleNum, 0, 0);

	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoBlendState();
	shaderMgr.UndoSamplerState();

	// SRVのバインド解除
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DevCon->VSSetShaderResources(0, 1, &nullSRV);
	DevCon->PSSetShaderResources(1, 1, &nullSRV);
}
