// Stable Fluids の各工程を1枚のPSで描く(g_passで切り替え)。t0=主入力、t1=副入力
Texture2D		g_tex0 : register(t0);
Texture2D		g_tex1 : register(t1);
SamplerState	g_smp  : register(s0);

cbuffer cbFluid : register(b0)
{
	float2	g_texelSize;	// 1 / シミュレーション解像度
	float	g_dt;
	float	g_dissipation;	// 毎秒の減衰率

	float	g_aspect;		// 幅 / 高さ
	int		g_pass;
	float2	_blank1;

	float2	g_splatPos;		// 0〜1の画面座標
	float	g_splatRadius;	// 画面の高さに対する比率
	float	_blank2;

	float3	g_splatValue;	// 速度ならxy、濃度ならx
	float	_blank3;
};

struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV  : TEXCOORD0;
};

// FluidSimulator.hのPassと値を合わせる
#define PASS_ADVECT				0
#define PASS_SPLAT_VELOCITY		1
#define PASS_SPLAT_DENSITY		2

// 入力位置を中心にしたガウス分布(円形になるよう縦横比を補正)
float SplatWeight(float2 uv)
{
	float2 p = (uv - g_splatPos) * float2(g_aspect, 1.0);

	return exp(-dot(p, p) / (g_splatRadius * g_splatRadius));
}

// 既定のブレンドがAlphaなので、出力のwは常に1にする
float4 main(VSOutput In) : SV_Target0
{
	float2 uv = In.UV;

	if (g_pass == PASS_ADVECT)
	{
		// 速度(texel/秒)で逆向きにたどり、その位置の値(t1)を取る
		float2 vel = g_tex0.Sample(g_smp, uv).xy;
		float2 prevUV = uv - vel * g_dt * g_texelSize;

		float3 value = g_tex1.Sample(g_smp, prevUV).xyz;

		return float4(value * exp(-g_dissipation * g_dt), 1.0);
	}

	if (g_pass == PASS_SPLAT_VELOCITY)
	{
		float3 base = g_tex0.Sample(g_smp, uv).xyz;

		return float4(base + g_splatValue * SplatWeight(uv), 1.0);
	}

	// PASS_SPLAT_DENSITY：重ねるほど濃くなるが1は超えない
	float base = g_tex0.Sample(g_smp, uv).x;
	float amount = saturate(g_splatValue.x * SplatWeight(uv));

	return float4(base + (1.0 - base) * amount, 0.0, 0.0, 1.0);
}
