#include "inc_KdGPUParticle.hlsli"

// パーティクル本体バッファ(読み書き)
RWStructuredBuffer<Particle> g_ParticleBuffer : register(u0);

cbuffer cbGPUParticleInit : register(b0)
{
	uint	g_MaxParticleNum;	// パーティクルの最大数
	float3	_pad;
};

//================================
// 全パーティクルを「死亡」状態で初期化する
// ※スレッド数はC++側でMaxParticleNumぶん(256の倍数に切り上げ)Dispatchする
//================================
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint index = id.x;

	// バッファ範囲外のスレッドは何もしない(256刻みDispatchの端数対策)
	if (index >= g_MaxParticleNum) { return; }

	Particle p = (Particle)0;
	p.Life = 0; // 0以下 = 死亡状態

	g_ParticleBuffer[index] = p;
}
