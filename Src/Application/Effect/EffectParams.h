#pragma once

#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"
#include <vector>
#include <string>
#include <type_traits>

// パーティクルの発生方式
//	※GPUParticleLayer::Countの意味がこのモードによって変わる点に注意
//	  (Burst：1回の発生イベントで出す個数／Continuous：1秒あたりに発生させる個数)
enum class ParticleEmitMode
{
	Burst,		// 各LayerのCount個を一度に発生。EmitInterval(秒)ごとに繰り返す(0以下なら再生開始時の1回のみ)
	Continuous,	// 各LayerのCount個/秒のペースで、再生中ずっと発生させ続ける
};

// パーティクルをどの描画パスから描画するか(ビットフラグ)
//	3D描画パス側は、従来の「不透明・半透明を描いた後にライティング結果へ合成するDrawLit」に加え、
//	「ブルーム(発光)ポストシェーダーの入力になるDrawBloom」の2箇所からEffectInstance::Draw()を
//	呼び出すようになる。EffectInstance/EffectDispatcher/EffectEditorはこの値を見て、
//	今呼ばれているのが自分の望むパスに含まれるかどうかを判定し、含まれる時だけ実際に描画する
//	(呼び出し元は毎フレーム両方のパスから1回ずつ呼ぶだけで良く、判定は全てこちら側で行う)。
//
//	ビットフラグにしているのは、1つのエフェクトが「通常描画(Lit)もしつつ発光(Bloom)もする」
//	ように、複数のパスへ同時に描画したいケースがある為。仮に排他的なenumのままだと、
//	将来パスの種類が増えるたびに組み合わせ用の値(LitAndBloom等)を都度追加する羽目になるが、
//	ビットフラグならKdHasDrawPassFlag()での判定だけで任意の組み合わせに対応できる
enum class ParticleDrawPass : uint8_t
{
	Default = 1 << 0,	// 通常の合成(従来通りの見え方。DrawLitから描画される)
	Blight = 1 << 1,	// ブルームポストシェーダーの入力として描画され、発光して見える(DrawBloomから描画される)
};

inline ParticleDrawPass operator|(ParticleDrawPass a, ParticleDrawPass b)
{
	using T = std::underlying_type_t<ParticleDrawPass>;
	return static_cast<ParticleDrawPass>(static_cast<T>(a) | static_cast<T>(b));
}
inline ParticleDrawPass operator&(ParticleDrawPass a, ParticleDrawPass b)
{
	using T = std::underlying_type_t<ParticleDrawPass>;
	return static_cast<ParticleDrawPass>(static_cast<T>(a) & static_cast<T>(b));
}
inline ParticleDrawPass& operator|=(ParticleDrawPass& a, ParticleDrawPass b)
{
	a = a | b;
	return a;
}

// flagsにtestのビットが1つでも立っていればtrue
//	(testに単一のパス値を渡す使い方を想定。EffectInstance::Draw(pass)の判定に使う)
inline bool KdHasDrawPassFlag(ParticleDrawPass flags, ParticleDrawPass test)
{
	using T = std::underlying_type_t<ParticleDrawPass>;
	return (static_cast<T>(flags) & static_cast<T>(test)) != 0;
}

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

	// Shape(速度・サイズ・寿命・色)ぶんだけのEmitParameterを作る。
	// BillboardMode/StretchScale(Layer単位の値)はここでは設定されないので、
	// 呼び出し元がGPUParticleLayer::ToEmitParameter()経由で使うか、
	// 自前で追加設定すること
	KdGPUParticle::EmitParameter ToEmitParameter(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 汎用エフェクトの1層分(形状 + そのレイヤーのEmit数 + ビルボード方式)
//	Count の意味はGPUParticleParams::EmitModeに従う
//	(Burst：1回の発生イベントで出す個数／Continuous：1秒あたりに発生させる個数)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct GPUParticleLayer
{
	DirectionalEmitShape	Shape;
	int		Count = 30;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// ストレッチビルボード関連
	//	BillboardMode==Stretchの場合、このLayerから発生する全パーティクルが
	//	速度方向へ伸びる板ポリになる(KdGPUParticle_VS.hlsl側で実際に計算される)。
	//	StretchScaleは「速度→伸び量」の係数で、値が大きいほどよく伸びる。
	//	伸び量そのもののクランプ範囲は暴走防止のためVS側の固定値を使う
	//	(意図的にLayer単位のパラメータにはしていない。EffectInstance.h等のコメント参照)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	KdParticleBillboardMode	BillboardMode = KdParticleBillboardMode::Normal;
	float					StretchScale = 0.15f;

	// Shape由来のEmitParameterに、このLayerのBillboardMode/StretchScaleを合成して返す。
	// EffectInstance側は基本的にlayer.Shape.ToEmitParameter()ではなくこちらを呼ぶこと
	KdGPUParticle::EmitParameter ToEmitParameter(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const;
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

	ParticleEmitMode	EmitMode = ParticleEmitMode::Burst;
	float	EmitInterval = 0.0f;	// Burst：再発生までの間隔(秒)。0以下なら自動リピートしない

	std::vector<GPUParticleLayer>	Layers = { GPUParticleLayer{} };	// 最低1層

	DirectX::SimpleMath::Vector3	Gravity = { 0.0f, -0.5f, 0.0f };

	std::string	TexturePath;	// Asset/Textureからの相対パス

	KdParticleBlendMode BlendMode = KdParticleBlendMode::Add;	// KdGPUParticle::Draw()へそのまま渡され、実際に切り替わる

	// どの描画パス(DrawLit/DrawBloom)から描画されるか(ビットフラグ、複数同時可)。
	// デフォルトはLitのみ(従来通りの見え方)
	ParticleDrawPass DrawPassFlags = ParticleDrawPass::Default;

	// Continuous、またはBurstでEmitInterval>0の場合はtrue(=再生中はStop()が必要になる)
	bool IsLooping() const;
};