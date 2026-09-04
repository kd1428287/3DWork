#pragma once

#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"
#include <vector>
#include <string>

// パーティクルのブレンドモード(将来的な描画方式切り替え用)
enum class KdParticleBlendMode
{
	Add,	// 加算合成(現状のKdGPUParticleのデフォルト挙動)
	Alpha,	// 半透明合成
};

// パーティクルの発生方式
//	※GPUParticleLayer::Countの意味がこのモードによって変わる点に注意
//	  (Burst：1回の発生イベントで出す個数／Continuous：1秒あたりに発生させる個数)
enum class KdParticleEmitMode
{
	Burst,		// 各LayerのCount個を一度に発生。EmitInterval(秒)ごとに繰り返す(0以下なら再生開始時の1回のみ)
	Continuous,	// 各LayerのCount個/秒のペースで、再生中ずっと発生させ続ける
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 発生方向(baseDir)を軸にした「1層分」の速度・サイズ・寿命・色の形状定義
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// VelocityMin = baseDir * DirScaleMin - OffsetMin
// VelocityMax = baseDir * DirScaleMax + OffsetMax
// baseDirが{0,0,0}で呼ばれた場合はOffsetMin〜OffsetMaxの範囲のみになるため、
// 方向を使わない単純エフェクト(FootDust等)にもそのまま使える
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct DirectionalEmitShape
{
	float	DirScaleMin = 1.0f;
	float	DirScaleMax = 4.0f;
	DirectX::SimpleMath::Vector3	OffsetMin = { 1.0f, 0.5f, 1.0f };
	DirectX::SimpleMath::Vector3	OffsetMax = { 1.0f, 1.5f, 1.0f };

	float	SizeMin = 0.02f;
	float	SizeMax = 0.06f;

	float	LifeMin = 0.45f;
	float	LifeMax = 0.8f;

	DirectX::SimpleMath::Vector4	ColorStartMin = { 0.1f, 0.1f, 0.1f, 1.0f };
	DirectX::SimpleMath::Vector4	ColorStartMax = { 0.1f, 0.1f, 0.1f, 1.0f };

	DirectX::SimpleMath::Vector4	ColorMin = { 0.1f, 0.85f, 0.4f, 1.0f };
	DirectX::SimpleMath::Vector4	ColorMax = { 0.1f, 0.85f, 0.4f, 1.0f };

	KdGPUParticle::EmitParameter ToEmitParameter(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 汎用エフェクトの1層分(形状 + そのレイヤーのEmit数)
//	Count の意味はGPUParticleParams::EmitModeに従う
//	(Burst：1回の発生イベントで出す個数／Continuous：1秒あたりに発生させる個数)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct GPUParticleLayer
{
	DirectionalEmitShape	Shape;
	int		Count = 30;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 1エフェクト分のパーティクル発生パラメータ(1層以上のLayersで構成)
//	※方向を伴わない単純エフェクトはEffectInstance::Play()/Emit()をbaseDir={0,0,0}で
//	  呼び出せば良い(各LayerのOffset範囲のみの等方分布として振る舞う)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct GPUParticleParams
{
	UINT	MaxParticleNum = 500;	// KdGPUParticle::Init()に渡す同時最大パーティクル数

	KdParticleEmitMode	EmitMode = KdParticleEmitMode::Burst;
	float	EmitInterval = 0.0f;	// Burst：再発生までの間隔(秒)。0以下なら自動リピートしない

	std::vector<GPUParticleLayer>	Layers = { GPUParticleLayer{} };	// 最低1層

	DirectX::SimpleMath::Vector3	Gravity = { 0.0f, -0.5f, 0.0f };

	std::string	TexturePath;	// Asset/Textureからの相対パス

	KdParticleBlendMode BlendMode = KdParticleBlendMode::Add;	// TODO: KdGPUParticle::Draw()側の対応待ち

	// Continuous、またはBurstでEmitInterval>0の場合はtrue(=再生中はStop()が必要になる)
	bool IsLooping() const;
};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 鍔迫り合いの火花のうち1層分(メイン/エンバー等)
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 形状はDirectionalEmitShapeを共用し、「パリィ/ガードで数が変わる」という
// WeaponClash特有の文脈だけをここに持たせる(通常のGPUParticleLayerとは
// Countの意味が異なる=状況依存の2値のため、無理に統合しない)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct DirectionalSparkLayer
{
	DirectionalEmitShape	Shape;

	UINT	CountParry = 120;	// パリィ成功時のEmit数
	UINT	CountBlock = 60;	// 通常ガードブロック時のEmit数

	KdGPUParticle::EmitParameter ToEmitParameter(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const
	{
		return Shape.ToEmitParameter(worldPos, baseDir);
	}
	UINT GetEmitCount(bool isParry) const { return isParry ? CountParry : CountBlock; }
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 鍔迫り合いの火花(WeaponClashEffectEvent)の全パラメータ
//	メイン(勢いよく飛ぶ火花)とエンバー(ゆっくり落ちるくすぶり)の2層構成。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct WeaponClashEffectParams
{
	UINT	MaxParticleNum = 2000;	// KdGPUParticle::Init()に渡す同時最大パーティクル数

	DirectionalSparkLayer	Main;	// 勢いよく飛ぶ火花
	DirectionalSparkLayer	Ember;	// ゆっくり落ちるくすぶり

	WeaponClashEffectParams();
};