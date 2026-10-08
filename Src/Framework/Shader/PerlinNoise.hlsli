#ifndef PERLIN_NOISE_HLSLI
#define PERLIN_NOISE_HLSLI

// PerlinNoise.h(CPU版)と同じ順列テーブル・同じ式。CPU側のPerlinNoiseGPUがテーブルを転送する。
// スロットはエンジンで未使用のtレジスタに合わせて変更すること(使うステージ側でも同じ番号でバインド)。
#ifndef PERLIN_PERM_SLOT
#define PERLIN_PERM_SLOT t10
#endif

StructuredBuffer<uint> g_perlinPerm : register(PERLIN_PERM_SLOT);

float PerlinFade(float t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

float PerlinGrad(uint hash, float x, float y, float z)
{
	uint h = hash & 15;
	float u = h < 8 ? x : y;
	float v = h < 4 ? y : ((h == 12 || h == 14) ? x : z);
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

// 3Dノイズ(出力はおおよそ-1〜1)
float PerlinNoise3D(float3 pos)
{
	float3 fl = floor(pos);
	int3 I = (int3)fl & 255; // 負数でもCPU版(int)floor & 255と同じ結果になる
	float3 f = pos - fl;

	float u = PerlinFade(f.x), v = PerlinFade(f.y), w = PerlinFade(f.z);

	int A = (int)g_perlinPerm[I.x] + I.y;
	int AA = (int)g_perlinPerm[A] + I.z;
	int AB = (int)g_perlinPerm[A + 1] + I.z;
	int B = (int)g_perlinPerm[I.x + 1] + I.y;
	int BA = (int)g_perlinPerm[B] + I.z;
	int BB = (int)g_perlinPerm[B + 1] + I.z;

	// z=0面 → z=1面の順に、x方向→y方向へ補間する
	float y0z0 = lerp(
		lerp(PerlinGrad(g_perlinPerm[AA], f.x, f.y, f.z), PerlinGrad(g_perlinPerm[BA], f.x - 1, f.y, f.z), u),
		lerp(PerlinGrad(g_perlinPerm[AB], f.x, f.y - 1, f.z), PerlinGrad(g_perlinPerm[BB], f.x - 1, f.y - 1, f.z), u), v);
	float y0z1 = lerp(
		lerp(PerlinGrad(g_perlinPerm[AA + 1], f.x, f.y, f.z - 1), PerlinGrad(g_perlinPerm[BA + 1], f.x - 1, f.y, f.z - 1), u),
		lerp(PerlinGrad(g_perlinPerm[AB + 1], f.x, f.y - 1, f.z - 1), PerlinGrad(g_perlinPerm[BB + 1], f.x - 1, f.y - 1, f.z - 1), u), v);
	return lerp(y0z0, y0z1, w);
}

// 2D入力版(z=0)
float PerlinNoise2D(float2 pos) { return PerlinNoise3D(float3(pos, 0.0)); }

// CPU版fbm(Vector2)と同じ。結果は-1〜1に正規化
float PerlinFbm2D(float2 pos, int octaves, float persistence = 0.5, float lacunarity = 2.0)
{
	float total = 0.0;
	float frequency = 1.0;
	float amplitude = 1.0;
	float maxValue = 0.0;

	for (int i = 0; i < octaves; ++i)
	{
		total += PerlinNoise3D(float3(pos * frequency, 0.0)) * amplitude;
		maxValue += amplitude;
		amplitude *= persistence;
		frequency *= lacunarity;
	}
	return total / maxValue;
}

#endif
