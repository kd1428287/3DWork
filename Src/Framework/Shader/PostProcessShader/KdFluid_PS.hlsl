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
	float	g_diffusion;	// 濃度の拡散係数(texel²/秒)
	float	g_vorticity;	// 渦度強調の強さ(1/秒)

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
#define PASS_DIVERGENCE			3
#define PASS_JACOBI				4
#define PASS_GRADIENT			5
#define PASS_CURL				6
#define PASS_VORTICITY_FORCE	7
#define PASS_DIFFUSE			8

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

	// 隣のテクセルへのオフセット(格子間隔=1テクセル)
	float2 dx = float2(g_texelSize.x, 0.0);
	float2 dy = float2(0.0, g_texelSize.y);

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

	if (g_pass == PASS_DIVERGENCE)
	{
		// t0 = 速度。中心差分で湧き出し量を求める(Yは下が正)
		float vL = g_tex0.Sample(g_smp, uv - dx).x;
		float vR = g_tex0.Sample(g_smp, uv + dx).x;
		float vU = g_tex0.Sample(g_smp, uv - dy).y;
		float vD = g_tex0.Sample(g_smp, uv + dy).y;

		return float4(0.5 * ((vR - vL) + (vD - vU)), 0.0, 0.0, 1.0);
	}

	if (g_pass == PASS_JACOBI)
	{
		// t0 = 圧力、t1 = 発散。隣の平均から発散を引いて反復する
		float pL = g_tex0.Sample(g_smp, uv - dx).x;
		float pR = g_tex0.Sample(g_smp, uv + dx).x;
		float pU = g_tex0.Sample(g_smp, uv - dy).x;
		float pD = g_tex0.Sample(g_smp, uv + dy).x;
		float div = g_tex1.Sample(g_smp, uv).x;

		return float4((pL + pR + pU + pD - div) * 0.25, 0.0, 0.0, 1.0);
	}

	if (g_pass == PASS_GRADIENT)
	{
		// t0 = 圧力、t1 = 速度。圧力の勾配を引いて発散を取り除く
		float pL = g_tex0.Sample(g_smp, uv - dx).x;
		float pR = g_tex0.Sample(g_smp, uv + dx).x;
		float pU = g_tex0.Sample(g_smp, uv - dy).x;
		float pD = g_tex0.Sample(g_smp, uv + dy).x;

		float2 vel = g_tex1.Sample(g_smp, uv).xy - 0.5 * float2(pR - pL, pD - pU);

		// 画面の端を壁にする(壁を貫く向きの速度を0にする)
		if (uv.x < g_texelSize.x || uv.x > 1.0 - g_texelSize.x) { vel.x = 0.0; }
		if (uv.y < g_texelSize.y || uv.y > 1.0 - g_texelSize.y) { vel.y = 0.0; }

		return float4(vel, 0.0, 1.0);
	}

	if (g_pass == PASS_CURL)
	{
		// t0 = 速度。回転の強さ(渦度)を中心差分で求める
		float vL = g_tex0.Sample(g_smp, uv - dx).y;
		float vR = g_tex0.Sample(g_smp, uv + dx).y;
		float uU = g_tex0.Sample(g_smp, uv - dy).x;
		float uD = g_tex0.Sample(g_smp, uv + dy).x;

		return float4(0.5 * ((vR - vL) - (uD - uU)), 0.0, 0.0, 1.0);
	}

	if (g_pass == PASS_VORTICITY_FORCE)
	{
		// t0 = 渦度、t1 = 速度。渦度の大きさの勾配(渦の中心向き)と直交する力を足す
		float c  = g_tex0.Sample(g_smp, uv).x;
		float wL = abs(g_tex0.Sample(g_smp, uv - dx).x);
		float wR = abs(g_tex0.Sample(g_smp, uv + dx).x);
		float wU = abs(g_tex0.Sample(g_smp, uv - dy).x);
		float wD = abs(g_tex0.Sample(g_smp, uv + dy).x);

		float2 grad = 0.5 * float2(wR - wL, wD - wU);
		float2 n = grad / max(length(grad), 1e-5);

		float2 vel = g_tex1.Sample(g_smp, uv).xy + g_vorticity * c * float2(n.y, -n.x) * g_dt;

		return float4(vel, 0.0, 1.0);
	}

	if (g_pass == PASS_DIFFUSE)
	{
		// t0 = 濃度。隣4マスの平均へ近づける(陽解法が発散しない上限で打ち切る)
		float c = g_tex0.Sample(g_smp, uv).x;
		float sum = g_tex0.Sample(g_smp, uv - dx).x + g_tex0.Sample(g_smp, uv + dx).x
				  + g_tex0.Sample(g_smp, uv - dy).x + g_tex0.Sample(g_smp, uv + dy).x;
		float a = min(g_diffusion * g_dt, 0.25);

		return float4(c + a * (sum - 4.0 * c), 0.0, 0.0, 1.0);
	}

	// PASS_SPLAT_DENSITY：重ねるほど濃くなるが1は超えない
	float base = g_tex0.Sample(g_smp, uv).x;
	float amount = saturate(g_splatValue.x * SplatWeight(uv));

	return float4(base + (1.0 - base) * amount, 0.0, 0.0, 1.0);
}
