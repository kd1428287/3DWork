// 墨の濃淡変換。inc_KdStandardShader.hlsli(g_Ink*)の後にインクルードすること

// 3Dのハッシュ(0〜1)
float InkHash(float3 p)
{
	p = frac(p * 0.3183099 + float3(0.71, 0.113, 0.419));
	p *= 17.0;
	return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 3Dバリューノイズ(0〜1)。3D空間で連続なので継ぎ目が出ない
float InkValueNoise(float3 x)
{
	float3 i = floor(x);
	float3 f = frac(x);
	f = f * f * (3.0 - 2.0 * f);

	return lerp(
		lerp(lerp(InkHash(i + float3(0, 0, 0)), InkHash(i + float3(1, 0, 0)), f.x),
			 lerp(InkHash(i + float3(0, 1, 0)), InkHash(i + float3(1, 1, 0)), f.x), f.y),
		lerp(lerp(InkHash(i + float3(0, 0, 1)), InkHash(i + float3(1, 0, 1)), f.x),
			 lerp(InkHash(i + float3(0, 1, 1)), InkHash(i + float3(1, 1, 1)), f.x), f.y),
		f.z);
}

// 墨のにじみ風ノイズ(0〜1)。座標を別のノイズで歪ませてから、広いむらと細かいむらを重ねる
float InkNoise(float3 wPos)
{
	float3 p = wPos * g_InkNoiseScale;

	float3 q = float3(InkValueNoise(p + 1.7), InkValueNoise(p + 8.3), InkValueNoise(p + 4.1));
	p += (q - 0.5) * 1.5;

	return InkValueNoise(p) * 0.65 + InkValueNoise(p * 2.3) * 0.35;
}

// 輝度を墨〜紙の範囲へ再マップする(設定はcbObjectのg_Ink*)。wPosは濃淡のゆらぎ用
float3 ApplyInkTone(float3 color, float3 wPos)
{
	float v = saturate(dot(color, float3(0.2126, 0.7152, 0.0722)) * g_InkLumaScale);

	v = pow(v, g_InkBias);
	v = saturate((v - 0.5) * g_InkContrast + 0.5);

	// 白寄りの面ほどゆらぎを強く効かせ、暗部の形は崩さない
	if (g_InkNoiseAmount > 0)
	{
		float w = smoothstep(0.5, 0.9, v);
		v = saturate(v + (InkNoise(wPos) - 0.5) * g_InkNoiseAmount * w);
	}

	// 段数指定時は境目だけ細く滑らかにして塗り分ける
	if (g_InkSteps > 0.5)
	{
		float f = v * g_InkSteps;
		v = saturate((floor(f) + smoothstep(0.45, 0.55, frac(f))) / g_InkSteps);
	}

	return lerp(color, lerp(g_InkBlack, g_InkPaper, v), g_InkAmount);
}
