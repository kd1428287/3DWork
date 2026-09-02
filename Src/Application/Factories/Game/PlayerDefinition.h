#pragma once

#include <string>
#include <vector>

#include "../../Components/Collision/ColliderComponent.h"   // ColliderCategory
#include "../../Components/Animation/ModelAnimatorComponent.h" // RootMotionAxis

// ============================================================
// PlayerFactoryが「何を作るか」を表すデータ。
// PlayerFactory自体はこの構造体が持つ値を組み立てるだけの責務に絞り、
// キャラクター固有の数値・パス・ボーン名を直接持たない。
//
// 将来的に敵キャラも同じ枠組みで作りたくなった場合、この構造体（の
// サブセット）をEnemyDefinition等と共通化しやすいように、
// 「見た目 / 物理 / 移動 / 武器」で塊を分けてある。
// ============================================================

// 見た目・アニメーション関連
struct PlayerVisualDefinition
{
	std::string modelPath;

	int animatorFPS = 60;
	std::string rootMotionBoneName;
	RootMotionAxis rootMotionAxis = RootMotionAxis::Y;
	float rootMotionAxisSign = -1.0f;
	float rootMotionScale = 0.01f;
};

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

// 体幹・HP等、戦闘まわりの数値
struct PlayerCombatStatsDefinition
{
	float maxHealth = 100.f;
	// PostureComponentの最大値・回復速度等も、必要になったらここに追加していく。
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

struct PlayerDefinition
{
	PlayerVisualDefinition visuals;
	PlayerCombatStatsDefinition combatStats;
	std::vector<CapsuleColliderDefinition> colliders;

	float walkSpeed = 2.0f;

	// ソケットだけ生成しておくボーン（将来の装備拡張・ボーン構成確認用）
	std::vector<std::string> auxiliarySocketBones;

	// 武器を取り付けるソケットのボーン名
	std::string weaponSocketBone;

	// プレイヤー本体・武器の両方に使う右腕IKチェーン
	IKChainDefinition rightArmIK;

	WeaponDefinition weapon;
};
