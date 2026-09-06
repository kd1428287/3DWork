#include "inc_KdPostProcessShader.hlsli"

Texture2D g_inputTex : register(t0);
SamplerState g_samLinear : register(s0); // 必要に応じてレジスタや定義を確認してください

// 露出などを渡すための定数バッファ（KdPostProcessShader::cbColorGradeInfo と同じレイアウト）
cbuffer cbColorGrade : register(b0)
{
	float g_exposure;
	float g_contrast;
	float g_saturation;
	float g_temperature; // -1(寒色/青) ～ +1(暖色/オレンジ)

	float g_tint; // -1(緑) ～ +1(マゼンタ)
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

// ホワイトバランス（色温度・Tint）の適用
// temperature : -1(寒色/青) ～ +1(暖色/オレンジ)
// tint        : -1(緑)      ～ +1(マゼンタ)
float3 ApplyWhiteBalance(float3 color, float temperature, float tint)
{
	// 色温度：Rチャンネルを上げ／Bチャンネルを下げる（またはその逆）ことで暖色・寒色を表現
	float3 warmFilter = float3(1.0f + temperature * 0.15f, 1.0f, 1.0f - temperature * 0.15f);
	color *= warmFilter;

	// Tint：Gチャンネルと R/Bチャンネルのバランスで緑～マゼンタを表現
	float3 tintFilter = float3(1.0f + tint * 0.1f, 1.0f - tint * 0.1f, 1.0f + tint * 0.1f);
	color *= tintFilter;

	return color;
}

// コントラストの適用（0.5を中心に伸縮）
float3 ApplyContrast(float3 color, float contrast)
{
	return saturate((color - 0.5f) * contrast + 0.5f);
}

// 彩度の適用（輝度とのブレンド）
float3 ApplySaturation(float3 color, float saturation)
{
	float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
	return lerp(luminance.xxx, color, saturation);
}

float4 main(VSOutput input) : SV_Target
{
    // 1. 直前の処理（DoFやBloom合成後）までの色を取得（リニア・HDR）
	float3 color = g_inputTex.Sample(g_samLinear, input.UV).rgb;

    // 2. 露出（Exposure）の適用
	color *= g_exposure;

    // 3. ホワイトバランス（色温度／Tint）の適用
	color = ApplyWhiteBalance(color, g_temperature, g_tint);

    // 4. ACES Filmic トーンマッピングの適用（HDR -> 0～1のLDRへ収める）
	color = ACESFilm(color);

    // 5. コントラストの適用（LDR化した後の方が破綻しにくい）
	color = ApplyContrast(color, g_contrast);

    // 6. 彩度の適用
	color = ApplySaturation(color, g_saturation);

    // 7. ガンマ補正（SRGB空間への変換）
	color = pow(saturate(color), 1.0f / 2.2f);

	return float4(color, 1.0f);
}
