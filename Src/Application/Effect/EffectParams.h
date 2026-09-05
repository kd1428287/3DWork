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
// 方向を使わない単純エフェクト(FootDust等)にもそのまま使える。
// 逆にDirScaleを大きくしOffsetを絞れば、鍔迫り合いの火花のような「特定方向へ勢いよく飛ぶ」
// 表現もこの1つの形状定義だけで表現できる(WeaponClash専用の形状は不要)。
//
// Color*は発生時(ColorStart)→消滅時(Color)への線形フェードを表す
// (KdGPUParticle::EmitParameter側が寿命に応じて補間する前提。ColorStartとColorを同じ値に
//  しておけば、フェードしない単色エフェクトとしても使える)
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
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 方向を伴わない単純エフェクト(HitSpark等)も、鍔迫り合いの火花のような
// 「発生方向に応じて勢いよく飛ぶ、複数層(Main/Ember等)構成」のエフェクトも、
// どちらもこの1つの構造体(Layersの組み合わせ)だけで表現する。
// ・方向を使わない場合：EffectInstance::Emit()/Play()/Update()をbaseDir={0,0,0}で呼べば、
//   各LayerはOffset範囲のみの等方分布として振る舞う
// ・方向を使う場合(鍔迫り合いの火花等)：baseDirを都度渡す。呼び出し側(EffectDispatcher等)が
//   イベントごとに異なる方向を計算して渡す想定
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