#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"
#include "inc_KdAura.hlsli"

// ゆらぎ用ノイズ(C++側 kAuraNoiseSlot / kAuraSamplerSlot と一致させる)
Texture2D g_auraNoiseTex : register(t12);
SamplerState g_auraSampler : register(s2);

// 外縁をノイズで削る最大量(輪郭の揺らぎ幅。大きいほど縁が大きく崩れる)
static const float kEdgeRagged = 0.7;

//================================
// ピクセルシェーダ：墨オーラ(膨らませた背面の外縁をノイズで揺らす。SV_Target0のみ)
//================================
float4 main(VSOutputAura In) : SV_Target0
{
	// カメラ位置はビュー行列から復元(行ベクトル規約: p_view = p_world * R + t)
	float3 camPos = -mul(g_mView[3].xyz, transpose((float3x3) g_mView));

	float3 N = normalize(In.wN);
	float3 V = normalize(camPos - In.wPos);

	float alpha = 1.0;

	// Width=0はシルエットのステンシル書き込み用なので、削らずそのまま描く
	[branch]
	if (g_AuraWidth > 0.0)
	{
		// 背面では外縁(殻の輪郭)で0、内側ほど大きくなる
		float edge = saturate(-dot(N, V));

		// ワールド座標のノイズを時間とともに上へ流す
		float2 uv = float2(In.wPos.x + In.wPos.z, In.wPos.y) * g_AuraNoiseScale;
		uv.y -= g_AuraTime * g_AuraScrollSpeed;
		float n = g_auraNoiseTex.Sample(g_auraSampler, uv).r;

		// ノイズが高い所ほど外縁が内側へ削れ、縁がギザギザに揺らぐ
		alpha = smoothstep(0.0, max(g_AuraSoftness, 0.001), edge - n * kEdgeRagged);
		clip(alpha - 0.01);
	}

	return float4(g_AuraColor * g_AuraIntensity, alpha);
}
