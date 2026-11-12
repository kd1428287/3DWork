#pragma once

#include "EffectParams.h"	// ParticleBlendMode / ParticleDrawPass / KdHasDrawPassFlag を流用する為
#include "Framework/Shader/SlashTrailShader/SlashTrailRenderer.h"	// SlashTrailVertex(=Vertex)の実体を流用する為

// トレイル(斬撃の軌跡)の描画用1頂点
using SlashTrailVertex = SlashTrailRenderer::Vertex;

// 記録された1サンプル(剣のTip/Base座標のペア)
struct SlashTrailSample
{
	Math::Vector3	Tip;
	Math::Vector3	Base;

	// 始点からのTip軌跡の累積距離
	float	Dist = 0.0f;

	// 平滑化済みの移動速度(単位/秒)
	float	Speed = 0.0f;
};

// 記録(Recorderが使う)
struct SlashTrailRecordParams
{
	// 直前のサンプルからこの距離以上動いたら記録する
	float	MinSampleDistance = 0.02f;

	// サンプル数の上限。到達したら追加を止める
	UINT	MaxSamples = 192;

	// 記録開始からこの秒数で自動終了する(0以下なら無期限。Endが来ない場合の保険)
	float	MaxRecordingTime = 10.0f;

	// 速度平滑化の時定数(秒)。フレームレートに依存しない
	float	SpeedSmoothingTime = 0.05f;
};

// 一筆の長さ・先端
struct SlashTrailStrokeParams
{
	// UVと色補間の基準長。始点からこの距離でu=1に達し、超えた分はu=1に固定される
	float	Length = 8.0f;

	// 先頭からこの距離のあいだ、幅を0→1へ絞る(0以下なら絞らない)
	float	HeadTaperLength = 0.6f;
};

// 速度による太さ変調
struct SlashTrailWidthParams
{
	// この速度(単位/秒)のとき太さ1倍
	float	SpeedReference = 12.0f;

	// 太さ倍率の下限・上限
	float	MinScale = 0.35f;
	float	MaxScale = 3.6f;
};

// 軌跡のスプライン平滑化(Catmull-Rom)
struct SlashTrailSplineParams
{
	bool	Enabled = true;

	// 1区間あたりの分割数
	UINT	Subdivisions = 16;

	// 細分化後の点数の上限(頂点バッファの容量にも使う)
	UINT	MaxPoints = 768;
};

// 消去：記録終了後、ノイズのしきい値を進めて全体を一括で消す(ディゾルブ)
//	ノイズの意味はパーティクルのParticleNoiseParamsと同じ(消去の進行度がLifeRate相当)
struct SlashTrailDissolveParams
{
	// 記録終了後、全体が消えるまでの時間(秒)
	float	Time = 0.75f;

	// Asset/Textures/Game/Effect/からの相対パス(タイリング可能なもの。Rチャンネルを使用)。
	// 空・解決失敗ならノイズを使わず、線形フェードで消える
	std::string	NoiseTexturePath = "Noise/ink_noise.png";

	// ノイズのタイリング(1ワールド単位あたりのタイル数)。長さ方向を粗く・幅方向を細かくすると、
	// 筆の進行方向に沿った筋状の消え方になる
	float	NoiseScaleU = 0.3f;	// 長さ方向
	float	NoiseScaleV = 3.0f;	// 幅方向

	// 消去の間に動くノイズUV量(タイル単位。Uは筆の進行方向に流れる)
	float	ScrollU = 0.0f;
	float	ScrollV = 0.0f;

	// 境界のなめらかさ
	float	Softness = 0.1f;

	// 消える境界の発光帯の幅
	float	EdgeWidth = 0.05f;

	// 発光帯の色と強さ(強さ0なら発光しない)
	DirectX::SimpleMath::Vector3	EdgeColor = { 1.0f, 0.5f, 0.1f };
	float	EdgeIntensity = 0.0f;
};

// 墨レイヤー(メインの帯)
struct SlashTrailInkParams
{
	// 始点側(u=0)から先頭側(u=1)へ補間する色
	DirectX::SimpleMath::Vector3	ColdColor = { 0.001f, 0.001f, 0.001f };
	DirectX::SimpleMath::Vector3	HotColor = { 0.001f, 0.001f, 0.001f };

	ParticleBlendMode	BlendMode = ParticleBlendMode::Alpha;
	ParticleDrawPass	DrawPassFlags = ParticleDrawPass::Default;
};

// 芯(コア)レイヤー：墨の上に重ねる、細く明るい加算合成の線
struct SlashTrailCoreParams
{
	bool	Enabled = true;

	// 墨の幅に対する倍率
	float	WidthScale = 0.3f;

	DirectX::SimpleMath::Vector3	Color = {0.2f,0.00f,0.00f};// { 0.45f, 0.325f, 0.3f };

	// 始点側のα(先頭側は1。0なら始点へ向かって薄れる)
	float	TailAlpha = 0.0f;

	ParticleBlendMode	BlendMode = ParticleBlendMode::Alpha;
	ParticleDrawPass	DrawPassFlags = ParticleDrawPass::Default;
};

// トレイル1本ぶんの定義(データ駆動で読み書きする単位)
struct SlashTrailParams
{
	SlashTrailRecordParams	Record;
	SlashTrailStrokeParams	Stroke;
	SlashTrailWidthParams	Width;
	SlashTrailSplineParams	Spline;
	SlashTrailDissolveParams	Dissolve;
	SlashTrailInkParams		Ink;
	SlashTrailCoreParams	Core;

	// Asset/Textures/Game/Effect/からの相対パス(パーティクルのTexturePathと同じ規約)。墨・芯で共用
	std::string	TexturePath = "Trail/Trail2_.png";
};
