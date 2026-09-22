#pragma once

#include "Application/Components/Physics/Collision/ColliderComponent.h" 
#include "CharacterConfigs.h"

// ============================================================
// Player/Enemy等、複数キャラクターのFactory/Definitionで共通して
// 使うデータ構造をまとめたヘッダ。
// キャラクター種別に依存しない「コライダー1本」「IKチェーン」「武器」の
// 定義をここに集約し、PlayerDefinition/EnemyDefinition等はこれを
// 部品として組み合わせる。
// ============================================================

// カプセルコライダー1本分の定義（Body / HurtBox 等）
struct CapsuleColliderDefinition
{
	std::string name;
	float radius = 0.0f;
	Math::Vector3 start;
	Math::Vector3 end;
	ColliderCategory category = ColliderCategory::Bump;

	// HurtBoxのように「自分がどのカテゴリと衝突判定を取るか」を絞る場合はここに指定。
	// 指定不要なら ColliderCategory::None のままにしておく。
	ColliderCategory interactsWith = ColliderCategory::None;

	bool isTrigger = false;
};

// TwoBoneIKComponentに渡す4ボーン分の定義
struct IKChainDefinition
{
	std::string rootBone;
	std::string midBone;
	std::string tipParentBone;
	std::string tipBone;
};

struct WeaponColliderDefinition
{
	Math::Vector3 halfExtents;
	Math::Vector3 offset;
};

struct WeaponDefinition
{
	std::string modelPath;

	Math::Vector3 socketLocalPosition;
	// Yaw, Pitch, Roll（度数）。
	// 注意: 既存実装ではYawだけ度数のままCreateFromYawPitchRollに渡され、
	// Pitchのみ XMConvertToRadians を通す非対称な呼び出しになっている
	// （PlayerFactory.cpp参照）。データ化にあたっても見た目を変えないよう、
	// 挙動はそのまま踏襲している。直すなら別タスクとして意図的に。
	Math::Vector3 socketLocalEulerRotationDeg;

	WeaponColliderDefinition hitBox;
};

// アニメーション1本分の再生設定
struct MotionClipData
{
	std::string animationName;
	float		duration		= -1.0f;
	bool		loop			= false;
	bool		useRootMotion	= false;
	float		blendDuration	= 0.1f;
	int			startFrame		= 0;
	int			endFrame		= 0;
};

// ============================================================
// HitReactionComponent(Player/Enemy共通)が被弾処理の中で発生させる
// 副作用のうち、キャラクターごとに変えたい値をまとめた設定。
// ============================================================

