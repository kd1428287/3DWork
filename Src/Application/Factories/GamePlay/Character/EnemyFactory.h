#pragma once

#include <string>
#include <unordered_map>

#include "Application/Definitions/Prefab/Prefab.h"

class GameObject;
class ObjectManager;

// ============================================================
// データ駆動のEnemyFactory(Prefab版)。
//
// PlayerFactoryと同じく、実際の組み立ては全てPrefabFactory/
// ComponentRegistryへ委譲する。EnemyFactory自身は
// 「enemyId → Prefab定義ファイルパス」の対応表を持ち、コンストラクタで
// 一括読み込みしておくだけの薄い窓口になる。
//
// 敵種ごとの見た目・当たり判定・AI設定(EnemyAIData)は全てPrefabの
// JSONファイル側に記述する(旧EnemyDefinitionは不要)。
// ============================================================
class EnemyFactory {
public:
	// definitionPathsは enemyId → Prefab JSONファイルパス の対応表。
	// 読み込みに失敗したエントリはスキップされ、ログに出力される。
	explicit EnemyFactory(const std::unordered_map<std::string, std::string>& definitionPaths);
	~EnemyFactory() = default;

	// コピー・ムーブ禁止(旧EnemyFactoryと同じ方針)。
	EnemyFactory(const EnemyFactory&) = delete;
	EnemyFactory& operator=(const EnemyFactory&) = delete;

	// 登録済みのenemyIdからGameObjectを生成する。未登録ならnullptr。
	GameObject* CreateEnemy(ObjectManager& objectManager, const std::string& enemyId, const Math::Vector3& position) const;

	bool IsKnownEnemy(const std::string& enemyId) const;

private:
	std::unordered_map<std::string, PrefabDefinition> definitions_;
};
