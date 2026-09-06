#include "inc_KdGPUParticle.hlsli"

// パーティクル本体バッファ(読み書き)
RWStructuredBuffer<Particle> g_ParticleBuffer : register(u0);

// リングバッファの書き込みカーソル(要素数1)
// ※発生のたびにインクリメントし、MaxParticleNumで折り返す事で
//   「一番古い(生きていても)パーティクルから上書きしていく」方式
//   AppendStructuredBuffer等の死亡リスト方式と違い、
//   カウンタ枯渇による未定義動作が起きないシンプルな実装
RWStructuredBuffer<uint> g_EmitCounter : register(u1);

cbuffer cbGPUParticleEmit : register(b0)
{
	float3 g_EmitPos; // 発生座標
	int g_EmitCount; // 今回発生させる数

	float3 g_EmitVelocityMin; // 初速の範囲(最小)
	float g_EmitSizeMin; // サイズの範囲(最小)

	float3 g_EmitVelocityMax; // 初速の範囲(最大)
	float g_EmitSizeMax; // サイズの範囲(最大)

	float4 g_EmitColorStartMin; // 色
	float4 g_EmitColorStartMax; // 色
	
	float4 g_EmitColorMin; // 色
	float4 g_EmitColorMax; // 色

	float g_EmitLifeMin; // 寿命の範囲(最小)
	float g_EmitLifeMax; // 寿命の範囲(最大)
	uint g_MaxParticleNum; // パーティクルの最大数
	float g_RandomSeed; // 乱数シード(毎回変える事)

	// ストレッチビルボード関連(追加分)
	// ※C++側 KdGPUParticle::cbEmit とレイアウトを必ず一致させる事
	float g_EmitBillboardMode; // 0:Normal 1:Stretch(そのままParticle::BillboardModeへコピーする)
	float g_EmitStretchScale; // Stretch時のみ使用：速度→伸び量の係数
	float2 _cbPad; // 16バイト境界に揃える為のパディング(C++側 cbEmit::_pad[2] に対応)
};

//================================
// 新規パーティクルの発生
// ※スレッド数はC++側でEmitCountぶん(256の倍数に切り上げ)Dispatchする
//================================
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	// 今回発生させる数を超えたスレッドは何もしない
	if ((int) id.x >= g_EmitCount)
	{
		return;
	}

	// リングバッファの書き込み位置を1つ確保
	uint rawIndex;
	InterlockedAdd(g_EmitCounter[0], 1, rawIndex);

	uint index = rawIndex % g_MaxParticleNum;

	// スレッド毎に異なる乱数シードを作る
	float seed = g_RandomSeed + (float) id.x * 0.918273f;

	Particle p;
	p.Position = g_EmitPos;
	p.Velocity = RandRange3(g_EmitVelocityMin, g_EmitVelocityMax, seed);
	p.Size = RandRange(g_EmitSizeMin, g_EmitSizeMax, seed + 3.0f);
	p.ColorStart = RandRange4(g_EmitColorStartMin, g_EmitColorStartMax, seed + 5.0f);
	p.Color = RandRange4(g_EmitColorMin, g_EmitColorMax, seed + 6.0f);
	p.LifeMax = RandRange(g_EmitLifeMin, g_EmitLifeMax, seed + 4.0f);
	p.Life = p.LifeMax;
	p.BillboardMode = g_EmitBillboardMode;
	p.StretchScale = g_EmitStretchScale;
	p._pad = 0.0f;

	g_ParticleBuffer[index] = p;
}
