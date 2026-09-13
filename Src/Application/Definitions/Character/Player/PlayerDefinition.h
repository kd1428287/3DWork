#pragma once

#include "../Common/CharacterDefinitionCommon.h"
// TODO 責務分離
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h" // RootMotionAxis

// ============================================================
// PlayerFactoryが「何を作るか」を表すデータ。
// PlayerFactory自体はこの構造体が持つ値を組み立てるだけの責務に絞り、
// キャラクター固有の数値・パス・ボーン名を直接持たない。
//
// コライダー/IKチェーン/武器等、Enemy等とも共通して使うデータ構造は
// CharacterDefinitionCommon.hに切り出してある。
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

// 体幹・HP等、戦闘まわりの数値
struct PlayerCombatStatsDefinition
{
	float maxHealth = 100.f;
	// PostureComponentの最大値・回復速度等も、必要になったらここに追加していく。
};

struct PlayerDefinition
{
	PlayerVisualDefinition visuals;
	PlayerCombatStatsDefinition combatStats;
	std::vector<CapsuleColliderDefinition> colliders;

	float walkSpeed = 4.0f;

	// ソケットだけ生成しておくボーン（将来の装備拡張・ボーン構成確認用）
	std::vector<std::string> auxiliarySocketBones;

	// 武器を取り付けるソケットのボーン名
	std::string weaponSocketBone;

	// プレイヤー本体・武器の両方に使う右腕IKチェーン
	IKChainDefinition rightArmIK;

	WeaponDefinition weapon;
};
