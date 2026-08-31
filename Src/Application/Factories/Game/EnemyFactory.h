#pragma once

#include "../GameObjectFactory.h"

class GameObject;
class EnemyDefinition;

// ============================================================
// データ駆動のEnemyFactory。
//
// 「敵の種類ごとのパラメータ(EnemyDefinition)」と「生成時に外から
// 渡したい実行時パラメータ(出現位置)」を分離している。
// database(id → EnemyDefinition)はコンストラクタで受け取り、
// 各エントリの生成ロジックをGameObjectFactory<Math::Vector3>へ登録する
// (Args = Math::Vector3で「出現位置」だけを実行時パラメータにしている。
//  複数体を出す時に個体差を付けたくなったら、Argsを増やすか、
//  EnemyDefinition側にランダム幅などのフィールドを足す形で拡張できる)。
//
// TerrainFactoryのコメントアウト例と同じ構造。database自体は
// Factory側で所有せず、const参照のまま各クロージャに生ポインタで
// キャプチャしている(database不変・Factoryより長生きする前提)。
//
// 実際のコンポーネント組み立て(BuildEnemy)は.cppに分離している。
// これにより、CreateEnemy()を呼ぶだけの側はTransformComponent/
// MovementComponent/EnemyStatusController/ColliderComponentの
// 実装詳細をincludeしなくて済む。
// ============================================================
class EnemyFactory {
public:
	// databaseは所有しない(この後変更されない前提)。
	// 呼び出し側がdatabaseの寿命をこのFactoryより長く保つこと。
	explicit EnemyFactory(const std::unordered_map<std::string, EnemyDefinition>& database);
	~EnemyFactory() = default;

	// コピー・ムーブ禁止(TerrainFactoryと同じ理由:
	// 各クロージャがdatabase内エントリへの生ポインタをキャプチャしているため、
	// このFactoryを複製すると寿命の前提が崩れる)。
	EnemyFactory(const EnemyFactory&) = delete;
	EnemyFactory& operator=(const EnemyFactory&) = delete;

	// 登録済みのenemyIdからGameObjectを生成する。未登録ならnullptr。
	GameObject* CreateEnemy(ObjectManager& objectManager, const std::string& enemyId, const Math::Vector3& position) {
		return registry_.Create(enemyId, objectManager, position);
	}

	bool IsKnownEnemy(const std::string& enemyId) const {
		return registry_.IsRegistered(enemyId);
	}

private:
	// 実際のコンポーネント組み立て。定義はEnemyFactory.cpp側。
	static GameObject* BuildEnemy(ObjectManager& objectManager, const EnemyDefinition& def, const Math::Vector3& position);

	GameObjectFactory<Math::Vector3> registry_;
};