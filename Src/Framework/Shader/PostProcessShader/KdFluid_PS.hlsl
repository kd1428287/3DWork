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

	float2	g_dstSize;		// 占有率の計測グリッドの解像度
	float2	_blank4;

	float	g_streamCenterX;	// 入口の中心(0〜1)
	float	g_streamHalfWidth;	// 入口の帯の半幅(画面の高さに対する比率)
	float	g_streamSpeed;		// 入口の流速(texel/秒)
	float	g_streamAngle;		// 入口の流れる向き(ラジアン。0で真下)

	float	g_streamDensity;	// 入口の濃度
	float	g_gravity;			// 墨が下へ引かれる力(texel/秒²)
	float	g_time;				// 経過時間(秒)
	float	g_ambient;			// ゆるやかな水流の強さ(texel/秒²)
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
#define PASS_DISPLAY			9
#define PASS_COVERAGE			10
#define PASS_EMIT_VELOCITY		11
#define PASS_EMIT_DENSITY		12
#define PASS_BUOYANCY			13
#define PASS_AMBIENT			14

// 墨の見た目の調整値
static const float3 PAPER_COLOR = float3(0.93, 0.91, 0.86);	// 紙の色(生成り)
static const float  PAPER_GRAIN = 0.06;						// 紙のざらつきの強さ
static const float  INK_ABSORB  = 3.5;						// 墨の濃さ(大きいほど濃度1で黒に近づく)
static const float  INK_MOTTLE  = 0.25;						// 紙の繊維による墨のムラ
static const float  INK_EDGE    = 0.35;						// 墨だまりの縁の締まり

float Hash(float2 p)
{
	p = frac(p * float2(123.34, 456.21));
	p += dot(p, p + 45.32);

	return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
	float2 i = floor(p);
	float2 f = frac(p);
	float2 u = f * f * (3.0 - 2.0 * f);

	return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), u.x),
				lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), u.x), u.y);
}

// 紙のざらつき(粗いムラから細かい繊維まで重ねる)。0〜1
float PaperNoise(float2 uv)
{
	float2 p = uv * float2(g_aspect, 1.0);

	return 0.5 * ValueNoise(p * 40.0) + 0.3 * ValueNoise(p * 160.0) + 0.2 * ValueNoise(p * 640.0);
}

// 入力位置を中心にしたガウス分布(円形になるよう縦横比を補正)
float SplatWeight(float2 uv)
{
	float2 p = (uv - g_splatPos) * float2(g_aspect, 1.0);

	return exp(-dot(p, p) / (g_splatRadius * g_splatRadius));
}

static const float INLET_TEXELS = 6.0;	// 入口の厚み(上端から何texelか)

// 上端の入口の重み：帯の中(横)で1、上端から離れる(縦)ほど薄れる
float StreamWeight(float2 uv)
{
	float dist = abs(uv.x - g_streamCenterX) * g_aspect;	// 画面の高さに対する比率
	float band = 1.0 - smoothstep(g_streamHalfWidth * 0.6, g_streamHalfWidth, dist);
	float zone = saturate(1.0 - uv.y / (g_texelSize.y * INLET_TEXELS));

	return band * zone;
}

static const float AMBIENT_SCALE = 1.5;	// 画面の高さあたりのノイズの格子数(小さいほどゆったりした流れ)

// ゆるやかな水流のもとになる、ゆっくり動くノイズ(大きさの違う2枚を重ねる)
float AmbientPotential(float2 pA, float2 pB)
{
	return ValueNoise(pA) + 0.5 * ValueNoise(pB);
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
		float pD = (uv.y + g_texelSize.y > 1.0) ? 0.0 : g_tex0.Sample(g_smp, uv + dy).x;	// 下端の外は圧力0(出口)
		float div = g_tex1.Sample(g_smp, uv).x;

		return float4((pL + pR + pU + pD - div) * 0.25, 0.0, 0.0, 1.0);
	}

	if (g_pass == PASS_GRADIENT)
	{
		// t0 = 圧力、t1 = 速度。圧力の勾配を引いて発散を取り除く
		float pL = g_tex0.Sample(g_smp, uv - dx).x;
		float pR = g_tex0.Sample(g_smp, uv + dx).x;
		float pU = g_tex0.Sample(g_smp, uv - dy).x;
		float pD = (uv.y + g_texelSize.y > 1.0) ? 0.0 : g_tex0.Sample(g_smp, uv + dy).x;	// 下端の外は圧力0(出口)

		float2 vel = g_tex1.Sample(g_smp, uv).xy - 0.5 * float2(pR - pL, pD - pU);

		// 左・右・上の端を壁にする(壁を貫く向きの速度を0にする)。下端は流れ出る出口として開ける
		if (uv.x < g_texelSize.x || uv.x > 1.0 - g_texelSize.x) { vel.x = 0.0; }
		if (uv.y < g_texelSize.y) { vel.y = 0.0; }

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

	if (g_pass == PASS_DISPLAY)
	{
		// t0 = 濃度(格子を線形補間で画面サイズへ拡大して読む)
		float d = g_tex0.Sample(g_smp, uv).x;

		float grain = PaperNoise(uv);

		float3 paper = PAPER_COLOR * (1.0 + (grain - 0.5) * PAPER_GRAIN);

		// 墨は紙の繊維になじむので、濃度に細かいムラを与える
		d *= 1.0 + (grain - 0.5) * INK_MOTTLE;

		// 濃いほど光を吸収する。青を少し残して、青みのある墨色にする
		float3 ink = exp(-d * INK_ABSORB * float3(1.0, 1.0, 0.9));

		// 濃度の変化が急な所(墨だまりの縁)を少し暗くする
		float2 grad = float2(g_tex0.Sample(g_smp, uv + dx).x - g_tex0.Sample(g_smp, uv - dx).x,
							 g_tex0.Sample(g_smp, uv + dy).x - g_tex0.Sample(g_smp, uv - dy).x);
		ink *= 1.0 - INK_EDGE * saturate(length(grad) * 8.0);

		return float4(paper * ink, 1.0);
	}

	if (g_pass == PASS_COVERAGE)
	{
		// t0 = 濃度。出力1マスが担当するブロックについて、墨が見える割合を平均する
		int2 p = int2(In.Pos.xy);
		int2 srcSize = int2(1.0 / g_texelSize + 0.5);
		int2 b0 = (p * srcSize) / int2(g_dstSize);
		int2 b1 = ((p + 1) * srcSize) / int2(g_dstSize);

		float sum = 0.0;

		for (int y = b0.y; y < b1.y; ++y)
		{
			for (int x = b0.x; x < b1.x; ++x)
			{
				sum += smoothstep(0.1, 0.5, g_tex0.Load(int3(x, y, 0)).x);
			}
		}

		return float4(sum / max((b1.x - b0.x) * (b1.y - b0.y), 1), 0.0, 0.0, 1.0);
	}

	if (g_pass == PASS_EMIT_VELOCITY)
	{
		// t0 = 速度。入口の帯の中を、ストリームの速度へ近づける
		float2 inletVel = float2(sin(g_streamAngle), cos(g_streamAngle)) * g_streamSpeed;
		float2 vel = lerp(g_tex0.Sample(g_smp, uv).xy, inletVel, StreamWeight(uv));

		return float4(vel, 0.0, 1.0);
	}

	if (g_pass == PASS_EMIT_DENSITY)
	{
		// t0 = 濃度。入口の帯の中を、ストリームの濃度へ近づける
		float d = lerp(g_tex0.Sample(g_smp, uv).x, g_streamDensity, StreamWeight(uv));

		return float4(d, 0.0, 0.0, 1.0);
	}

	if (g_pass == PASS_BUOYANCY)
	{
		// t0 = 速度、t1 = 濃度。墨は水より重いので、濃度に比例した力で下へ引かれる(Yは下が正)
		float2 vel = g_tex0.Sample(g_smp, uv).xy;
		vel.y += g_gravity * g_tex1.Sample(g_smp, uv).x * g_dt;

		return float4(vel, 0.0, 1.0);
	}

	if (g_pass == PASS_AMBIENT)
	{
		// t0 = 速度。ゆっくり変化する、渦状(発散のない)の弱い流れを足す
		float2 p = uv * float2(g_aspect, 1.0) * AMBIENT_SCALE;
		float2 pA = p + float2(0.05, 0.03) * g_time;
		float2 pB = p * 2.1 + float2(-0.04, 0.06) * g_time + 17.3;

		float e = 0.05;
		float2 ex = float2(e, 0.0);
		float2 ey = float2(0.0, e);

		// ノイズの勾配を90度回すと、湧き出しも吸い込みもない渦状の力になる
		float dpx = AmbientPotential(pA + ex, pB + ex * 2.1) - AmbientPotential(pA - ex, pB - ex * 2.1);
		float dpy = AmbientPotential(pA + ey, pB + ey * 2.1) - AmbientPotential(pA - ey, pB - ey * 2.1);
		float2 force = float2(dpy, -dpx) / (2.0 * e) * g_ambient;

		float2 vel = g_tex0.Sample(g_smp, uv).xy + force * g_dt;

		return float4(vel, 0.0, 1.0);
	}

	// PASS_SPLAT_DENSITY：重ねるほど濃くなるが1は超えない
	float base = g_tex0.Sample(g_smp, uv).x;
	float amount = saturate(g_splatValue.x * SplatWeight(uv));

	return float4(base + (1.0 - base) * amount, 0.0, 0.0, 1.0);
}
