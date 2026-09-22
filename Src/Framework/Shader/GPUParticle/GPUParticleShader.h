#pragma once

#include "ParticleBuffer.h"	// EmitParameter/ParticleBlendMode等の型を利用するため

//====================================================================
//
// GPUパーティクル用シェーダー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// ・CS(Init/Emit/Update)とVS/PS一式を保持する。全ParticleBufferインスタンスから
//   共有される(KdShaderManagerが唯一のインスタンスm_particleShaderとして生成する)
// ・Emit/Update用の定数バッファも、呼び出しの都度上書きして使う使い捨てデータのため
//   ここで共有する(KdShaderManagerのcbCamera等と同じ考え方)
// ・パーティクル本体バッファ(StructuredBuffer)自体はParticleBuffer側が個別に持ち、
//   UAV/SRVを引数として渡してくる
//
//====================================================================
class GPUParticleShader
{
public:

	GPUParticleShader() {}
	~GPUParticleShader() { Release(); }

	bool Init();
	void Release();

	// 指定UAVの全要素を「死亡」状態に初期化(ParticleBuffer::Init()から呼ばれる)
	void InitParticles(ID3D11UnorderedAccessView* particleUAV, UINT maxParticleNum);

	// count個ぶん発生させる
	void Emit(ID3D11UnorderedAccessView* particleUAV, ID3D11UnorderedAccessView* emitCounterUAV,
		UINT maxParticleNum, const ParticleBuffer::EmitParameter& param, UINT count);

	// 1フレームぶんのシミュレーション更新
	void Update(ID3D11UnorderedAccessView* particleUAV, UINT maxParticleNum,
		float deltaTime, const Math::Vector3& gravity);

	// 描画(ビルボードとしてインスタンシング描画)
	void Draw(ID3D11ShaderResourceView* particleSRV, UINT maxParticleNum,
		const std::shared_ptr<KdTexture>& texture, ParticleBlendMode blendMode);

private:

	bool CreateShaders();

	bool m_initialized = false;

	//================================================
	// シェーダー
	//================================================
	ID3D11ComputeShader* m_CS_Init = nullptr;
	ID3D11ComputeShader* m_CS_Emit = nullptr;
	ID3D11ComputeShader* m_CS_Update = nullptr;

	ID3D11VertexShader* m_VS = nullptr;
	ID3D11PixelShader* m_PS = nullptr;			// Add用(SV_Target0のみ)
	ID3D11PixelShader* m_PS_Masked = nullptr;	// Alpha用(SV_Target0+カラーグレード除外マスク)

	//================================================
	// 定数バッファ(全ParticleBufferで共有。使い捨てデータのためインスタンス毎に持つ必要がない)
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

		float			EmitBillboardMode = 0.0f;
		float			EmitStretchScale = 0.0f;
		float			_pad[2] = { 0.0f, 0.0f };	// 16バイト境界に揃える為のパディング
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