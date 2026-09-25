#include "Framework/KdFramework.h"

#include "GPUParticleShader.h"
#include "Framework/Shader/KdShaderManager.h"	// ※実際のパスに合わせて要調整

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダー・定数バッファの生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool GPUParticleShader::Init()
{
	Release();

	if (!CreateShaders()) { return false; }

	m_cb0_Init.Create();
	m_cb0_Emit.Create();
	m_cb0_Update.Create();

	m_initialized = true;

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンピュートシェーダー・描画用シェーダーの生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool GPUParticleShader::CreateShaders()
{
	ID3D11Device* Dev = KdDirect3D::Instance().WorkDev();

	// コンピュートシェーダー：初期化
	{
#include "GPUParticle_CS_Init.shaderInc"

		if (FAILED(Dev->CreateComputeShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_CS_Init)))
		{
			assert(0 && "GPUパーティクル：コンピュートシェーダー作成失敗(Init)");
			return false;
		}
	}

	// コンピュートシェーダー：発生
	{
#include "GPUParticle_CS_Emit.shaderInc"

		if (FAILED(Dev->CreateComputeShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_CS_Emit)))
		{
			assert(0 && "GPUパーティクル：コンピュートシェーダー作成失敗(Emit)");
			return false;
		}
	}

	// コンピュートシェーダー：更新
	{
#include "GPUParticle_CS_Update.shaderInc"

		if (FAILED(Dev->CreateComputeShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_CS_Update)))
		{
			assert(0 && "GPUパーティクル：コンピュートシェーダー作成失敗(Update)");
			return false;
		}
	}

	// 頂点シェーダー(頂点バッファ未使用のためInputLayoutは作成不要)
	{
#include "GPUParticle_VS.shaderInc"

		if (FAILED(Dev->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "GPUパーティクル：頂点シェーダー作成失敗");
			return false;
		}
	}

	// ピクセルシェーダー(Add用：SV_Target0のみ)
	{
#include "GPUParticle_PS.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS)))
		{
			assert(0 && "GPUパーティクル：ピクセルシェーダー作成失敗");
			return false;
		}
	}

	// ピクセルシェーダー(Alpha用：SV_Target0+カラーグレード除外マスク)
	{
#include "GPUParticle_PS_Masked.shaderInc"

		if (FAILED(Dev->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS_Masked)))
		{
			assert(0 && "GPUパーティクル：ピクセルシェーダー作成失敗(Masked)");
			return false;
		}
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void GPUParticleShader::Release()
{
	KdSafeRelease(m_CS_Init);
	KdSafeRelease(m_CS_Emit);
	KdSafeRelease(m_CS_Update);
	KdSafeRelease(m_VS);
	KdSafeRelease(m_PS);
	KdSafeRelease(m_PS_Masked);

	m_cb0_Init.Release();
	m_cb0_Emit.Release();
	m_cb0_Update.Release();

	m_initialized = false;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 指定UAVの全要素を「死亡」状態に初期化
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void GPUParticleShader::InitParticles(ID3D11UnorderedAccessView* particleUAV, UINT maxParticleNum)
{
	if (!m_initialized) { return; }

	m_cb0_Init.Work().MaxParticleNum = maxParticleNum;
	m_cb0_Init.Write();

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	DevCon->CSSetShader(m_CS_Init, nullptr, 0);
	DevCon->CSSetConstantBuffers(0, 1, m_cb0_Init.GetAddress());
	DevCon->CSSetUnorderedAccessViews(0, 1, &particleUAV, nullptr);

	UINT threadGroupNum = (maxParticleNum + 255) / 256;
	DevCon->Dispatch(threadGroupNum, 1, 1);

	ID3D11UnorderedAccessView* pNullUAV = nullptr;
	DevCon->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
	DevCon->CSSetShader(nullptr, nullptr, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 発生：リングバッファへcount個ぶん書き込む
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void GPUParticleShader::Emit(ID3D11UnorderedAccessView* particleUAV, ID3D11UnorderedAccessView* emitCounterUAV,
	UINT maxParticleNum, const ParticleBuffer::EmitParameter& param, UINT count)
{
	if (!m_initialized || count == 0) { return; }

	cbEmit& emit = m_cb0_Emit.Work();
	emit.EmitPos = param.Position;
	emit.EmitCount = (int)count;
	emit.EmitVelocityMin = param.VelocityMin;
	emit.EmitVelocityMax = param.VelocityMax;
	emit.EmitSizeStartMin = param.SizeStartMin;
	emit.EmitSizeStartMax = param.SizeStartMax;
	emit.EmitSizeEndMin = param.SizeEndMin;
	emit.EmitSizeEndMax = param.SizeEndMax;
	emit.EmitLifeMin = param.LifeMin;
	emit.EmitLifeMax = param.LifeMax;
	emit.EmitColorStartMin = param.ColorStartMin;
	emit.EmitColorStartMax = param.ColorStartMax;
	emit.EmitColorMin = param.ColorMin;
	emit.EmitColorMax = param.ColorMax;
	emit.MaxParticleNum = maxParticleNum;
	// 毎回変化する乱数シード(パーティクルが毎回同じ並びで発生しないようにする)
	emit.RandomSeed = static_cast<float>(rand() % 100000);
	emit.EmitBillboardMode = (param.BillboardMode == ParticleBillboardMode::Stretch) ? 1.0f : 0.0f;
	emit.EmitStretchScale = param.StretchScale;

	m_cb0_Emit.Write();

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	DevCon->CSSetShader(m_CS_Emit, nullptr, 0);
	DevCon->CSSetConstantBuffers(0, 1, m_cb0_Emit.GetAddress());

	ID3D11UnorderedAccessView* uavs[2] = { particleUAV, emitCounterUAV };
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
void GPUParticleShader::Update(ID3D11UnorderedAccessView* particleUAV, UINT maxParticleNum,
	float deltaTime, const Math::Vector3& gravity)
{
	if (!m_initialized) { return; }

	cbUpdate& update = m_cb0_Update.Work();
	update.MaxParticleNum = maxParticleNum;
	update.DeltaTime = deltaTime;
	update.Gravity = gravity;

	m_cb0_Update.Write();

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	DevCon->CSSetShader(m_CS_Update, nullptr, 0);
	DevCon->CSSetConstantBuffers(0, 1, m_cb0_Update.GetAddress());
	DevCon->CSSetUnorderedAccessViews(0, 1, &particleUAV, nullptr);

	UINT threadGroupNum = (maxParticleNum + 255) / 256;
	DevCon->Dispatch(threadGroupNum, 1, 1);

	ID3D11UnorderedAccessView* pNullUAV = nullptr;
	DevCon->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
	DevCon->CSSetShader(nullptr, nullptr, 0);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：ビルボードとしてインスタンシング描画
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void GPUParticleShader::Draw(ID3D11ShaderResourceView* particleSRV, UINT maxParticleNum,
	const std::shared_ptr<KdTexture>& texture, ParticleBlendMode blendMode)
{
	if (!m_initialized) { return; }

	ID3D11DeviceContext* DevCon = KdDirect3D::Instance().WorkDevContext();

	KdShaderManager& shaderMgr = KdShaderManager::Instance();

	// シェーダーのセット(頂点バッファを使わないためInputLayoutは不要＝nullptrでOK)
	shaderMgr.SetVertexShader(m_VS);
	DevCon->IASetInputLayout(nullptr);

	// Alphaブレンド時のみ、カラーグレード除外マスクを書き込むPSへ切り替える
	shaderMgr.SetPixelShader(blendMode == ParticleBlendMode::Alpha ? m_PS_Masked : m_PS);

	// パーティクル本体バッファをVS用SRVとしてセット
	DevCon->VSSetShaderResources(0, 1, &particleSRV);

	// パーティクル用テクスチャをPS用SRVとしてセット
	if (texture)
	{
		DevCon->PSSetShaderResources(1, 1, texture->WorkSRViewAddress());
	}

	// 通常テクスチャ用サンプラーをセット
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp, 0);

	// ブレンドモードの切り替え・Z書き込み無効(重なった時に不透明に潰れないように)
	const KdBlendState blendState = [&]() {
		switch (blendMode)
		{
		case ParticleBlendMode::Add:
			return KdBlendState::Add;
		case ParticleBlendMode::Alpha:
			return KdBlendState::AlphaMasked;
		case ParticleBlendMode::Multiply:
			return KdBlendState::Multiply;
		default:
			break;
		}
		}(); 
	shaderMgr.ChangeBlendState(blendState);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	DevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	DevCon->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

	// パーティクル1粒＝4頂点(板ポリ) × 最大パーティクル数ぶんをインスタンシング描画
	DevCon->DrawInstanced(4, maxParticleNum, 0, 0);

	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoBlendState();
	shaderMgr.UndoSamplerState();

	// SRVのバインド解除
	ID3D11ShaderResourceView* nullSRV = nullptr;
	DevCon->VSSetShaderResources(0, 1, &nullSRV);
	DevCon->PSSetShaderResources(1, 1, &nullSRV);
}