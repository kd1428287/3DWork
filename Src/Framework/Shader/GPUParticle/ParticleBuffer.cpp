#include "Framework/KdFramework.h"

#include "ParticleBuffer.h"
#include "GPUParticleShader.h"
#include "Framework/Shader/KdShaderManager.h"	// ※実際のパスに合わせて要調整

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// バッファの生成、パーティクルの初期化(初期化処理自体はGPUParticleShader側に委譲)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool ParticleBuffer::Init(UINT maxParticleNum)
{
	Release();

	m_maxParticleNum = maxParticleNum;

	if (!CreateBuffers(maxParticleNum)) { return false; }

	// 全パーティクルを「死亡」状態に初期化
	KdShaderManager::Instance().m_particleShader.InitParticles(m_particleUAV, maxParticleNum);

	m_initialized = true;

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// バッファ(パーティクル本体・発生カウンタ)の生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool ParticleBuffer::CreateBuffers(UINT maxParticleNum)
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
// 解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ParticleBuffer::Release()
{
	KdSafeRelease(m_particleUAV);
	KdSafeRelease(m_particleSRV);
	KdSafeRelease(m_particleBuffer);

	KdSafeRelease(m_emitCounterUAV);
	KdSafeRelease(m_emitCounterBuffer);

	m_initialized = false;
	m_maxParticleNum = 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 発生・更新・描画：実処理は共有シェーダー(GPUParticleShader)へ委譲するだけ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ParticleBuffer::Emit(const EmitParameter& param, UINT count)
{
	if (!m_initialized || count == 0) { return; }

	KdShaderManager::Instance().m_particleShader.Emit(
		m_particleUAV, m_emitCounterUAV, m_maxParticleNum, param, count);
}

void ParticleBuffer::Update(float deltaTime, const Math::Vector3& gravity)
{
	if (!m_initialized) { return; }

	KdShaderManager::Instance().m_particleShader.Update(m_particleUAV, m_maxParticleNum, deltaTime, gravity);
}

void ParticleBuffer::Draw(const std::shared_ptr<KdTexture>& texture, ParticleBlendMode blendMode)
{
	if (!m_initialized) { return; }

	KdShaderManager::Instance().m_particleShader.Draw(m_particleSRV, m_maxParticleNum, texture, blendMode);
}