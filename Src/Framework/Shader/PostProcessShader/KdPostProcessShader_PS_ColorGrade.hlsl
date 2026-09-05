#include "inc_KdPostProcessShader.hlsli"

Texture2D g_inputTex : register(t0);
SamplerState g_samLinear : register(s0); // 必要に応じてレジスタや定義を確認してください

cbuffer cbColorGrade : register(b0)
{
	float g_exposure;
	float g_contrast;
	float g_saturation;
	float g_temperature;

	float g_tint;
	float3 _blank;
};

float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(VSOutput input) : SV_Target
{
    // 1. 直前の処理（DoFやBloom合成後）までの色を取得
	float3 color = g_inputTex.Sample(g_samLinear, input.UV).rgb;

    // 2. 露出（Exposure）の適用
	color *= g_exposure;

    // 3. ACES Filmic トーンマッピングの適用
	color = ACESFilm(color);

    // 4. ガマ補正（SRGB空間への変換）
	color = pow(color, 1.0f / 2.2f);

	return float4(color, 1.0f);
}
