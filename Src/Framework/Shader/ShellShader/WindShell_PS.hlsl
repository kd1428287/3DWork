#include "../inc_KdCommon.hlsli"
#include "../GPUParticle/inc_GPUParticleNoise.hlsli"	// g_noiseTex(t2) / g_wrapSS(s1) / ApplyParticleNoise
#include "inc_WindShell.hlsli"

// 筋テクスチャ(縦に引き伸ばしたノイズ。U方向にタイルできるもの)
Texture2D g_streakTex : register(t1);

//================================
// ピクセルシェーダ出力
// SV_Target0 … カラー
// SV_Target1 … カラーグレード/DoF除外マスク(トレイルと同じ扱い)
//================================
struct PSOutput
{
	float4 Color : SV_Target0;
	float GradeMask : SV_Target1;
};

PSOutput main(WindVSOut In)
{
	// 筋のしきい値切り抜き
	float n = g_streakTex.Sample(g_wrapSS, In.StreakUV).r;
	float mask = smoothstep(g_Threshold - g_Softness, g_Threshold + g_Softness, n);

	// 輪郭付近を濃くして、中央が面に見えないようにする
	float3 vCam = g_CamPos - In.wPos;
	float camDist = length(vCam);
	float rim = pow(1.0 - saturate(abs(dot(normalize(In.wN), vCam / camDist))), g_RimPower);

	float4 outColor = g_Color;
	outColor.a *= mask * lerp(0.3, 1.0, rim) * In.Fade * g_Alpha;

	// ディゾルブ無効時は線形フェード、有効時はノイズで消す(トレイルと同じ)
	if (!(g_NoiseFlags & NOISE_DISSOLVE)) { outColor.a *= g_LifeRate; }
	outColor = ApplyParticleNoise(outColor, In.ShellUV, g_LifeRate, float2(0.0, 0.0));

	// 距離フォグ(遠景で薄れる)
	if (g_DistanceFogEnable) { outColor.a *= saturate(1.0 / exp(camDist * g_DistanceFogDensity)); }

	PSOutput Out;
	Out.Color = outColor;
	Out.GradeMask = saturate(outColor.a);
	return Out;
}
