#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"
#include "inc_KdAura.hlsli"

// ゆらぎ用ノイズ(C++側 kAuraNoiseSlot / kAuraSamplerSlot と一致させる)
Texture2D g_auraNoiseTex : register(t12);
SamplerState g_auraSampler : register(s2);

//================================
// ピクセルシェーダ：オーラ(g_AuraKind 0:リムライト型 / 1:炎型。SV_Target0のみ)
//================================
float4 main(VSOutputAura In) : SV_Target0
{
	// カメラ位置はビュー行列から復元(行ベクトル規約: p_view = p_world * R + t)
	float3 camPos = -mul(g_mView[3].xyz, transpose((float3x3) g_mView));

	float3 N = normalize(In.wN);
	float3 V = normalize(camPos - In.wPos);

	// 縁ほど強い(両タイプ共通)
	float fres = pow(saturate(1.0 - dot(N, V)), g_AuraFresnelPower);
	float scroll = g_AuraTime * g_AuraScrollSpeed;

	float3 col;

	[branch]
	if (g_AuraKind == 1)
	{
		// 輪郭側ほど見せる(正面は薄く残す)
		float side = lerp(0.15, 1.0, fres);

		// 高さ(0:根元 1:先端)
		float h = saturate((In.wPos.y - g_AuraBaseY) / max(g_AuraHeight, 0.001));

		// 縦に引き伸ばしたUVで、時間とともに上へ流す
		float2 uv = float2(In.wPos.x + In.wPos.z, In.wPos.y * 0.5) * g_AuraNoiseScale;

		// 低周波ノイズで横に揺らす
		float sway = g_auraNoiseTex.Sample(g_auraSampler, uv * 0.4 + float2(0.0, -scroll * 0.5)).r - 0.5;
		uv.x += sway * 0.5;
		uv.y -= scroll;
		float n = g_auraNoiseTex.Sample(g_auraSampler, uv).r;

		// 高いほどしきい値が上がり、ノイズの高い所だけが先細りの舌として残る
		float flame = n - h;
		float mask = smoothstep(0.0, max(g_AuraSoftness, 0.001), flame);

		// 芯ほど明るく、縁は濃い色
		float core = saturate(flame / 0.4);
		col = lerp(g_AuraColor, g_AuraCoreColor, core) * mask * side;
	}
	else
	{
		// ワールド座標でノイズを引き、時間とともに上へ流す
		float2 uv = float2(In.wPos.x + In.wPos.z, In.wPos.y) * g_AuraNoiseScale;
		uv.y -= scroll;
		float n = g_auraNoiseTex.Sample(g_auraSampler, uv).r;

		float mask = saturate(fres * (0.4 + n * 1.2));
		col = g_AuraColor * mask;
	}

	return float4(col * g_AuraIntensity, 1.0);
}
