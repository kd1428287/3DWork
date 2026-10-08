// 衝撃波(画面の歪み+色収差)。入力はカラーグレード済みの全画面画像

// ※既存の共通hlsliにVS出力構造体があるなら、この定義を消してそちらをincludeする
struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV : TEXCOORD0;
};

Texture2D g_tex : register(t0);
SamplerState g_ss : register(s0);

cbuffer cbDistortion : register(b0)
{
	float ChromaticAmount;
	float ShockwaveWidth;
	float Aspect;
	int ShockwaveNum;

	float4 Shockwave[4]; // xy=中心UV z=半径 w=強さ
};

float4 main(VSOutput In) : SV_Target0
{
	float2 uv = In.UV;
	float2 offset = 0;

	for (int i = 0; i < ShockwaveNum; ++i)
	{
		float2 d = uv - Shockwave[i].xy;
		d.x *= Aspect; // 縦横比を補正して真円にする

		float dist = length(d);

		// リングの中心(半径)からの距離で山形のマスクを作る
		float ring = 1.0 - saturate(abs(dist - Shockwave[i].z) / ShockwaveWidth);
		ring = smoothstep(0.0, 1.0, ring);

		float2 dir = d / max(dist, 1e-4);
		dir.x /= Aspect; // UV空間へ戻す

		offset += dir * ring * Shockwave[i].w;
	}

	// 歪みの強い場所ほどRGBのサンプル位置をずらす(リングが虹色に縁取られる)
	float4 g = g_tex.Sample(g_ss, uv - offset);
	float r = g_tex.Sample(g_ss, uv - offset * (1.0 + ChromaticAmount)).r;
	float b = g_tex.Sample(g_ss, uv - offset * (1.0 - ChromaticAmount)).b;

	return float4(r, g.g, b, g.a);
}
