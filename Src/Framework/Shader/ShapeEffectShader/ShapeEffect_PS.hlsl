// ShapeEffectShader::cbShape と一致させる事(VSと同じ並び)
cbuffer cbShapeEffect : register(b5)
{
	row_major float4x4	g_mWorld;
	float4				g_Color;
};

Texture2D		g_tex : register(t0);
SamplerState	g_ss  : register(s0);

struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV  : TEXCOORD0;
};

// テクスチャ × 色(alphaは動きのalphaが乗算済み)
float4 main(VSOutput In) : SV_Target0
{
	return g_tex.Sample(g_ss, In.UV) * g_Color;
}
