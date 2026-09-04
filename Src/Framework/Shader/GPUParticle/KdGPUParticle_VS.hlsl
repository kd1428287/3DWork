#include "../inc_KdCommon.hlsli"
#include "inc_KdGPUParticle.hlsli"

// パーティクル本体バッファ(読み取り専用)
StructuredBuffer<Particle> g_ParticleBuffer : register(t0);

// 板ポリの四隅のオフセット(中心からの相対位置)
static const float2 kQuadOffset[4] =
{
	float2(-0.5f,  0.5f),
	float2( 0.5f,  0.5f),
	float2(-0.5f, -0.5f),
	float2( 0.5f, -0.5f),
};

// 板ポリの四隅のUV座標(kQuadOffsetと対応)
static const float2 kQuadUV[4] =
{
	float2(0, 0),
	float2(1, 0),
	float2(0, 1),
	float2(1, 1),
};

//================================
// 頂点シェーダ：頂点バッファを使わず、
// SV_VertexID(0～3)とSV_InstanceID(パーティクル番号)だけで
// カメラ方向を向いた板ポリ(ビルボード)を1枚組み立てる
//================================
VSOutput main(uint vertID : SV_VertexID, uint instID : SV_InstanceID)
{
	VSOutput Out = (VSOutput)0;

	Particle p = g_ParticleBuffer[instID];

	// 死亡中のパーティクルは面積0にして描画結果に影響しないようにする
	float size = (p.Life > 0) ? p.Size : 0;

	// ビュー行列(row_major)の列から、カメラのワールド空間での右方向・上方向を取り出す
	float3 camRight = float3(g_mView._11, g_mView._21, g_mView._31);
	float3 camUp    = float3(g_mView._12, g_mView._22, g_mView._32);

	float3 wPos = p.Position
		+ camRight * kQuadOffset[vertID].x * size
		+ camUp    * kQuadOffset[vertID].y * size;

	Out.Pos = mul(float4(wPos, 1), g_mView);
	Out.Pos = mul(Out.Pos, g_mProj);

	Out.UV = kQuadUV[vertID];
	Out.LifeRate = (p.LifeMax > 0) ? saturate(p.Life / p.LifeMax) : 0;
	Out.Color = lerp(p.ColorStart, p.Color, 1 - Out.LifeRate); 

	return Out;
}
