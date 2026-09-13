#include "inc_KdPostProcessShader.hlsli"

Texture2D g_inputTex : register(t0);
Texture2D g_colorGradeMaskTex : register(t1); // 追加：カラーグレード除外マスク(R8_UNORM。0=適用,1=除外)
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
	float3 srcColor = g_inputTex.Sample(g_samLinear, input.UV).rgb;

	// カラーグレード除外マスク(0=通常通り適用、1=完全除外)
	float mask = g_colorGradeMaskTex.Sample(g_samLinear, input.UV).r;

	//------------------------------------------------------------
	// 経路A：通常のグレーディング(露出・WB・トーンマップ・コントラスト・彩度・ガンマ)
	//------------------------------------------------------------
	float3 gradedColor = srcColor;

    // 2. 露出（Exposure）の適用
	gradedColor *= g_exposure;

    // 3. ホワイトバランス（色温度／Tint）の適用
	gradedColor = ApplyWhiteBalance(gradedColor, g_temperature, g_tint);

    // 4. ACES Filmic トーンマッピングの適用（HDR -> 0～1のLDRへ収める）
	gradedColor = ACESFilm(gradedColor);

    // 5. コントラストの適用（LDR化した後の方が破綻しにくい）
	gradedColor = ApplyContrast(gradedColor, g_contrast);

    // 6. 彩度の適用
	gradedColor = ApplySaturation(gradedColor, g_saturation);

    // 7. ガンマ補正（SRGB空間への変換）
	gradedColor = pow(saturate(gradedColor), 1.0f / 2.2f);

	//------------------------------------------------------------
	// 経路B：マスク対象(パーティクル等)用。
	// 露出・ホワイトバランス・コントラスト・彩度の"スタイライズ"は
	// 一切適用しないが、ACESトーンマッピングとガンマ補正だけは
	// 経路Aと共通で通す。ここを省略してsrcColorをそのまま出すと、
	// HDR値が未変換のまま出力され白飛び・不自然な明るさになるため注意。
	//------------------------------------------------------------
	float3 unGradedColor = ACESFilm(srcColor);
	unGradedColor = pow(saturate(unGradedColor), 1.0f / 2.2f);

	// mask=1の場所だけ経路Bを使う(パーティクルの不透明度に応じて連続的に混ざる)
	float3 finalColor = lerp(gradedColor, unGradedColor, mask);

	return float4(finalColor, 1.0f);
}
