// 墨の液体合成PS：ぼかした密度(t0)をしきい値で塊にし、濃淡と滲みを付けて出力する

Texture2D g_densityTex : register(t0);
SamplerState g_ss : register(s0);

cbuffer cbLiquid : register(b0)
{
	float3	g_InkColor;		// 塊の中心の色(淡墨)
	float	g_Threshold;	// 密度のしきい値

	float3	g_EdgeColor;	// 縁の色(濃墨)
	float	g_Softness;		// しきい値の境界の柔らかさ

	float	g_EdgeWidth;	// 縁として濃くなる密度の幅
	float	g_HaloAlpha;	// しきい値の外側に出す滲みの濃さ
	float2	_blank;
};

struct PSInput
{
	float4 Pos : SV_Position;
	float2 UV : TEXCOORD0;
};

struct PSOutput
{
	float4 Color : SV_Target0;
	float  Mask : SV_Target1;	// カラーグレード・DoF除外マスク
};

PSOutput main(PSInput In)
{
	float d = g_densityTex.Sample(g_ss, In.UV).r;

	float body = smoothstep(g_Threshold - g_Softness, g_Threshold + g_Softness, d);
	float halo = smoothstep(g_Threshold * 0.5, g_Threshold, d) * g_HaloAlpha;
	float alpha = max(body, halo);

	// 全画面描画のため、墨の無い画素はマスクを含め何も書かない
	clip(alpha - 0.004);

	float edge = 1.0 - smoothstep(g_Threshold, g_Threshold + g_EdgeWidth, d);

	PSOutput Out;
	Out.Color = float4(lerp(g_InkColor, g_EdgeColor, edge), alpha);
	Out.Mask = alpha;
	return Out;
}
