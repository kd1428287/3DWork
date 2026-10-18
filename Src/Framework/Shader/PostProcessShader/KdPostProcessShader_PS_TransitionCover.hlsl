// 画面遷移の覆い。UI描画後にAlphaブレンドで全画面へ重ねる

struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV : TEXCOORD0;
};

cbuffer cbTransition : register(b0)
{
	float2 Center;
	float Progress;
	float Time;

	float3 FadeColor;
	float Aspect;

	float MaxDist;
	float WaveAmount;
	float WaveFreq;
	float RingWidth;

	float ChromaticAmount;
	float Softness;
	float2 _pad;
};

float4 main(VSOutput In) : SV_Target0
{
	float2 c = In.UV - Center;
	c.x *= Aspect;
	float d = length(c) / MaxDist;

	// Progress=1のとき、全画素でfront-d >= Softnessとなり必ずcover=1になる
	float front = Progress * (1.0 + Softness);
	float cover = saturate((front - d) / max(Softness, 1e-4));

	return float4(FadeColor, cover);
}
