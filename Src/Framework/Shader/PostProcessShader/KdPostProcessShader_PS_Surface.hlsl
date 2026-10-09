// 質感テクスチャを明暗として画面へ合成するポストプロセス
// ※ VSOutputは既存のKdPostProcessShader_PS_*.hlslと同じ定義に合わせること
struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV  : TEXCOORD0;
};

Texture2D    g_srcTex     : register(t0); // シーン画像
Texture2D    g_surfaceTex : register(t1); // 質感(グレー。0.5=変化なし)
SamplerState g_ss         : register(s0);

cbuffer cbSurface : register(b0)
{
	float2 g_Tiling;    // 画面に対する繰り返し数
	float2 g_Offset;    // UVスクロール量

	float  g_Intensity; // 全体の強さ(0〜1)
	float  g_LumaLow;   // これ以下の明るさでは質感が最大
	float  g_LumaHigh;  // これ以上の明るさでは質感が消える
	float  _blank;
};

float4 main(VSOutput In) : SV_Target0
{
	float4 src = g_srcTex.Sample(g_ss, In.UV);

	// 折り返し境界でミップが乱れないよう、折り返す前のUVの微分を使う
	float2 rawUV = In.UV * g_Tiling + g_Offset;
	float t = g_surfaceTex.SampleGrad(g_ss, frac(rawUV), ddx(rawUV), ddy(rawUV)).r;

	// A：SoftLight(Pegtop式)。t=0.5で無変化。HDR値で暴れないよう0〜1で計算する
	float3 b = saturate(src.rgb);
	float3 soft = (1.0 - 2.0 * t) * b * b + 2.0 * t * b;

	// B：明るい部分ほど質感を弱める
	float luma = dot(src.rgb, float3(0.2126, 0.7152, 0.0722));
	float w = 1.0 - smoothstep(g_LumaLow, g_LumaHigh, luma);

	// 変化分だけを元のHDR色へ足す
	float3 rgb = src.rgb + (soft - b) * w * g_Intensity;

	return float4(rgb, src.a);
}
