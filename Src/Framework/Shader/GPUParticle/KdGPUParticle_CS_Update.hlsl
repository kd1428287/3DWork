#include "inc_KdGPUParticle.hlsli"

// パーティクル本体バッファ(読み書き)
RWStructuredBuffer<Particle> g_ParticleBuffer : register(u0);

cbuffer cbGPUParticleUpdate : register(b0)
{
	uint	g_MaxParticleNum;	// パーティクルの最大数
	float	g_DeltaTime;		// 前フレームからの経過時間
	float2	_pad0;

	float3	g_Gravity;			// 重力(加速度)
	float	_pad1;
};

//================================
// 生存中のパーティクルを1フレームぶんシミュレーションする
// ※スレッド数はC++側でMaxParticleNumぶん(256の倍数に切り上げ)Dispatchする
//================================
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint index = id.x;

	if (index >= g_MaxParticleNum) { return; }

	Particle p = g_ParticleBuffer[index];

	// 死亡中のパーティクルは何もしない
	if (p.Life <= 0) { return; }

	// 重力を速度に加算
	p.Velocity += g_Gravity * g_DeltaTime;

	// 移動
	p.Position += p.Velocity * g_DeltaTime;

	// 寿命を減らす
	p.Life -= g_DeltaTime;
	if (p.Life < 0) { p.Life = 0; }

	g_ParticleBuffer[index] = p;
}
