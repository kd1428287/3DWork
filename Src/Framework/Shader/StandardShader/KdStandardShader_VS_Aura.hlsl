#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"
#include "inc_KdAura.hlsli"

//================================
// 頂点シェーダ：オーラ(スキニング + 法線方向への押し出し + 上への持ち上げ)
//================================
VSOutputAura main(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float4 color : COLOR,
	float3 normal : NORMAL,
	float3 tangent : TANGENT,
	uint4 skinIndex : SKININDEX,
	float4 skinWeight : SKINWEIGHT)
{
	// スキニング(VS_Litと同じ)
	if (g_IsSkinMeshObj)
	{
		row_major float4x4 mBones = 0;
		[unroll]
		for (int i = 0; i < 4; i++)
		{
			mBones += g_mBones[skinIndex[i]] * skinWeight[i];
		}

		pos = mul(pos, mBones);
		normal = mul(normal, (float3x3) mBones);
	}

	float3 wN = normalize(mul(normal, (float3x3) g_mWorld));
	float4 wPos = mul(pos, g_mWorld);

	// 法線方向へ押し出し、上を向く面ほど上へ持ち上げて炎の背丈を作る
	wPos.xyz += wN * g_AuraWidth;
	wPos.y += g_AuraRise * saturate(wN.y + 0.3);

	VSOutputAura Out;
	Out.wPos = wPos.xyz;
	Out.wN = wN;
	Out.Pos = mul(mul(wPos, g_mView), g_mProj);

	return Out;
}
