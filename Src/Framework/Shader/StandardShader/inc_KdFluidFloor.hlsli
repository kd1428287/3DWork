// 床の墨。KdStandardShader_PS_LitFluidでのみ使う(g_ssの宣言の後にインクルードすること)
Texture2D g_fluidTex : register(t14);

// KdStandardShader::cbFluidFloorと並びを一致させること
cbuffer cbFluidFloor : register(b12)
{
	float2	g_FluidOrigin;		// 流体マップのワールドXZ最小値
	float2	g_FluidSize;		// 流体マップのワールドXZの大きさ

	float	g_FluidAbsorb;		// 墨の濃さ
	float	g_FluidFloorY;		// 床の高さ
	float	g_FluidFloorBand;	// 床とみなす高さの幅
	float	_fluidBlank;
};

// 確認用(0:通常 1:マップのUVを表示 2:濃度を表示)
static const int kFluidDebug = 0;

// 床の面(上向き・指定の高さ・マップ内)にだけ、濃度に応じて墨を掛ける
float3 ApplyFluidFloor(float3 color, float3 wPos, float3 wN)
{
	float2 uv = (wPos.xz - g_FluidOrigin) / g_FluidSize;
	float2 inside = step(0.0, uv) * step(uv, 1.0);
	float up = smoothstep(0.85, 0.95, normalize(wN).y);
	float onFloor = 1.0 - smoothstep(0.0, g_FluidFloorBand, abs(wPos.y - g_FluidFloorY));

	float mask = inside.x * inside.y * up * onFloor;
	float density = g_fluidTex.SampleLevel(g_ss, saturate(uv), 0).x;

	if (kFluidDebug == 1) { return lerp(color, float3(saturate(uv), 0.0), mask); }
	if (kFluidDebug == 2) { return lerp(color, density.xxx, mask); }

	// Displayパスと同じ吸収式(青を少し残す)
	return color * exp(-density * mask * g_FluidAbsorb * float3(1.0, 1.0, 0.9));
}
