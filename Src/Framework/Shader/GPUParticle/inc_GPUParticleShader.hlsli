// パーティクル1粒のデータ
// ※C++側(ParticleBuffer::Particle)とフィールドの並び順を完全に一致させる事。
//   HLSLは「1変数が16バイト境界をまたがない」規則で暗黙のパディングを挿入するため、
//   並び順がC++と1つでもズレると構造体の実バイトサイズがC++側のsizeof()と食い違い、
//   StructuredBufferの要素サイズ計算が壊れる(隣の要素へフィールドが漏れて書き込まれる)
struct Particle
{
	float3 Position; // ワールド座標
	float Life; // 残り寿命(秒) 0以下で死亡扱い

	float3 Velocity; // 速度
	float SizeStart; // 開始時表示サイズ(板ポリの一辺の長さ)

	float4 ColorStart; // 発生直後の熱い色
	float4 Color; // 冷えた後の最終色

	float LifeMax; // 発生時の寿命(フェードアウト計算に使用)

	// ストレッチビルボード関連(以前は_pad3だった領域を転用)
	float BillboardMode; // 0:Normal(カメラ正面) 1:Stretch(速度方向に伸びる)
	float StretchScale; // Stretch時のみ使用：速度→伸び量の係数
	float SizeEnd; // 終了時表示サイズ(板ポリの一辺の長さ。旧_pad転用領域)
};

// 描画シェーダー(VS→PS)の受け渡し用
struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV : TEXCOORD0;
	float4 Color : TEXCOORD1;
	float LifeRate : TEXCOORD2; // 0(死亡直前)～1(発生直後)
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

float4 RandRange4(float4 minValue, float4 maxValue, float seed)
{
	float4 t = float4(Rand(seed), Rand(seed + 1.234f), Rand(seed + 2.468f), Rand(seed + 3.702f));
	return lerp(minValue, maxValue, t);
}

float RandRange(float minValue, float maxValue, float seed)
{
	return lerp(minValue, maxValue, Rand(seed));
}
