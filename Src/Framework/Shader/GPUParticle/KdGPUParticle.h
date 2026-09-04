#pragma once

//====================================================================
//
// GPUパーティクル
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// ・パーティクルデータはGPU上のStructuredBufferで保持し、CPUへは戻さない
// ・発生(Emit)／更新(Update)はコンピュートシェーダーで実行
// ・描画は頂点バッファを使わず、SV_VertexID/SV_InstanceIDと
//   StructuredBufferの読み取り(SRV)だけでビルボードを組み立てる
//
// 【使い方】
//   KdGPUParticle particle;
//   particle.Init(10000);
//   :
//   // 毎フレーム
//   particle.Emit(emitParam, 50);      // 発生させたい時だけ呼ぶ
//   particle.Update(deltaTime);        // シミュレーション更新
//   particle.Draw(pTexture);           // 描画(3D描画パスの最後、半透明描画のタイミングで)
//
//====================================================================
class KdGPUParticle
{
public:

	KdGPUParticle() {}
	~KdGPUParticle() { Release(); }

	// パーティクル1粒のデータ
	// ※HLSL側(inc_KdGPUParticle.hlsli の Particle構造体)とレイアウトを必ず一致させる事
	struct Particle
	{
		Math::Vector3	Position;
		float			Life = 0.0f;

		Math::Vector3	Velocity;
		float			Size = 0.1f;

		Math::Vector4	ColorStart = { 0.f,0.f,0.f, 1.0f };	// 発生直後の熱い色(1.0超で明るめに)
		Math::Vector4	Color = { 0.f,0.f,0.f, 1.0f };						// 冷えた後の最終色

		float			LifeMax = 1.0f;
		float			_pad[3] = { 0,0,0 };
	};

	// 発生パラメータ
	struct EmitParameter
	{
		Math::Vector3	Position;

		Math::Vector3	VelocityMin = { -1,-1,-1 };
		Math::Vector3	VelocityMax = { 1, 1, 1 };

		float			SizeMin = 0.1f;
		float			SizeMax = 0.3f;

		float			LifeMin = 0.5f;
		float			LifeMax = 1.5f;

		Math::Vector4	ColorStartMin = { 2.0f, 2.0f, 1.5f, 1.0f };
		Math::Vector4	ColorStartMax = { 2.0f, 2.0f, 1.5f, 1.0f };

		Math::Vector4	ColorMin = { 0.f, 0.f, 0.f, 1.0f };
		Math::Vector4	ColorMax = { 0.f, 0.f, 0.f, 1.0f };
	};

	//================================================
	// 初期化・解放
	//================================================

	// maxParticleNum：同時に存在できる最大パーティクル数
	bool Init(UINT maxParticleNum = 10000);

	void Release();

	//================================================
	// 更新・描画
	//================================================

	// パーティクルをcount個発生させる(リングバッファへ書き込み)
	void Emit(const EmitParameter& param, UINT count);

	// 1フレームぶんのシミュレーション更新(コンピュートシェーダーで実行)
	void Update(float deltaTime, const Math::Vector3& gravity = { 0.0f, -0.5f, 0.0f });

	// 描画(加算合成のビルボードとして描画)
	// ※事前にKdShaderManager::WriteCBCamera等でカメラ情報の転送が済んでいる事
	void Draw(const std::shared_ptr<KdTexture>& texture);

	UINT GetMaxParticleNum() const { return m_maxParticleNum; }

private:

	bool CreateBuffers(UINT maxParticleNum);
	bool CreateShaders();

	bool m_initialized = false;

	UINT m_maxParticleNum = 0;

	//================================================
	// シェーダー
	//================================================
	ID3D11ComputeShader* m_CS_Init = nullptr;
	ID3D11ComputeShader* m_CS_Emit = nullptr;
	ID3D11ComputeShader* m_CS_Update = nullptr;

	ID3D11VertexShader* m_VS = nullptr;
	ID3D11PixelShader* m_PS = nullptr;

	//================================================
	// バッファ
	//================================================

	// パーティクル本体(CS：UAVで読み書き／VS：SRVで読み取り)
	ID3D11Buffer* m_particleBuffer = nullptr;
	ID3D11UnorderedAccessView* m_particleUAV = nullptr;
	ID3D11ShaderResourceView* m_particleSRV = nullptr;

	// 発生用リングバッファの書き込みカーソル(要素数1)
	ID3D11Buffer* m_emitCounterBuffer = nullptr;
	ID3D11UnorderedAccessView* m_emitCounterUAV = nullptr;

	//================================================
	// 定数バッファ
	//================================================
	struct cbInit
	{
		UINT	MaxParticleNum = 0;
		float	_pad[3] = { 0,0,0 };
	};
	KdConstantBuffer<cbInit> m_cb0_Init;

	struct cbEmit
	{
		Math::Vector3	EmitPos;
		int				EmitCount = 0;

		Math::Vector3	EmitVelocityMin;
		float			EmitSizeMin = 0.0f;

		Math::Vector3	EmitVelocityMax;
		float			EmitSizeMax = 0.0f;

		Math::Vector4	EmitColorStartMin = { 2.0f, 2.0f, 1.5f, 1.0f };
		Math::Vector4	EmitColorStartMax = { 2.0f, 2.0f, 1.5f, 1.0f };

		Math::Vector4	EmitColorMin = { 1,1,1,1 };
		Math::Vector4	EmitColorMax = { 1,1,1,1 };

		float			EmitLifeMin = 0.0f;
		float			EmitLifeMax = 0.0f;
		UINT			MaxParticleNum = 0;
		float			RandomSeed = 0.0f;
	};
	KdConstantBuffer<cbEmit> m_cb0_Emit;

	struct cbUpdate
	{
		UINT	MaxParticleNum = 0;
		float	DeltaTime = 0.0f;
		float	_pad0[2] = { 0,0 };

		Math::Vector3	Gravity;
		float			_pad1 = 0.0f;
	};
	KdConstantBuffer<cbUpdate> m_cb0_Update;
};
