#pragma once
#include "EnemyAIData.h"
#include "../Common/CharacterCollisionDefaults.h"
#include "../Common/CharacterDefinitionCommon.h" // HitReactionConfig

// ============================================================
// 敵1種類分のパラメータ。CSV/JSON等の外部データから読み込んで組み立てる
// ============================================================

// EnemyFactory::CreateAIController()がこれを見てAddComponentする型を切り替える
enum class EnemyType
{
	Brute,
	Warrock,
};

struct EnemyDefinition
{
	std::string name = "Enemy"; // GameObjectの表示名

	std::string modelPath = "Asset/Models/Character/Brute/Brute.gltf";
	EnemyType type = EnemyType::Brute;

	float characterScale = 1.0f;

	EnemyAIData aiData;

	HitReactionConfig hitReactionConfig;

	// --- 体格関連の算出値 ---------------------------------------------
	float GetColliderRadius() const {
		return CharacterCollisionDefaults::kBodyWidth * 0.5f * characterScale;
	}

	Math::Vector3 GetModelScale() const {
		return Math::Vector3(characterScale, characterScale, characterScale);
	}
};