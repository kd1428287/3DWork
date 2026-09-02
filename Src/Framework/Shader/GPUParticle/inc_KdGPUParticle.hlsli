//=====================================================
//
// GPUパーティクル 共通定義
// ※C++側 KdGPUParticle::Particle 構造体とレイアウトを必ず一致させること
//
//=====================================================

// パーティクル1粒のデータ
struct Particle
{
	float3	Position;	// ワールド座標
	float	Life;		// 残り寿命(秒) 0以下で死亡扱い

	float3	Velocity;	// 速度
	float	Size;		// 表示サイズ(板ポリの一辺の長さ)

	float4	Color;		// 色(RGBA)

	float	LifeMax;	// 発生時の寿命(フェードアウト計算に使用)
	float3	_pad;
};

// 描画シェーダー(VS→PS)の受け渡し用
struct VSOutput
{
	float4	Pos			: SV_Position;
	float2	UV			: TEXCOORD0;
	float4	Color		: TEXCOORD1;
	float	LifeRate	: TEXCOORD2;	// 0(死亡直前)～1(発生直後)
};

//--------------------------------------------------
// 疑似乱数(ハッシュベース) 0～1を返す
// ※厳密な乱数ではないが、パーティクルのばらつき程度の用途には十分
//--------------------------------------------------
float Rand(float seed)
{
	return frac(sin(seed * 12.9898f) * 43758.5453f);
}

float3 RandRange3(float3 minValue, float3 maxValue, float seed)
{
	float3 t = float3(Rand(seed), Rand(seed + 1.234f), Rand(seed + 2.468f));
	return lerp(minValue, maxValue, t);
}

float RandRange(float minValue, float maxValue, float seed)
{
	return lerp(minValue, maxValue, Rand(seed));
}
