// 画面遷移の波紋(歪み+色収差)。入力はDistortion後の全画面画像。覆いは別パス(TransitionCover)

struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV : TEXCOORD0;
};

Texture2D g_tex : register(t0);
SamplerState g_ss : register(s0);

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
	float2 uv = In.UV;

	float2 c = uv - Center;
	c.x *= Aspect;
	float dist = length(c);
	float d = dist / MaxDist; // 中心から最遠の角までを0〜1に正規化

	// 波面の位置。Coverパスと同じ式にして、歪みの帯と覆いの縁を一致させる
	float front = Progress * (1.0 + Softness);

	// 波面付近だけ強い帯(x*xで負数のpowを避ける)
	float x = (d - front) / RingWidth;
	float ring = exp(-x * x);

	float2 dir = c / max(dist, 1e-4);
	dir.x /= Aspect;

	float2 offset = dir * sin(d * WaveFreq - Time * 12.0) * WaveAmount * ring;

	float4 g = g_tex.Sample(g_ss, uv - offset);
	float r = g_tex.Sample(g_ss, uv - offset * (1.0 + ChromaticAmount)).r;
	float b = g_tex.Sample(g_ss, uv - offset * (1.0 - ChromaticAmount)).b;

	return float4(r, g.g, b, g.a);
}
