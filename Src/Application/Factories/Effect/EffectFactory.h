#pragma once

#include "../GameObjectFactory.h"

class GameObject;
class EffectDefinition;

// ============================================================
// データ駆動のEffectFactory。EnemyFactoryと同じ構造。
//
// 「エフェクトの種類ごとのパラメータ(EffectDefinition)」と
// 「生成時に外から渡したい実行時パラメータ(発生位置・向き)」を
// 分離している。database(id → EffectDefinition)はコンストラクタで
// 受け取り、各エントリの生成ロジックを
// GameObjectFactory<Math::Vector3, Math::Quaternion>へ登録する。
//
// 向き(Math::Quaternion)を実行時引数に含めているのは、斬撃のように
// 発生方向に依存するエフェクトを想定しているため。向きが不要な
// エフェクト(汎用パーティクル等)は呼び出し側でIdentityを渡せばよい。
// 個体差(ランダムな寿命ブレ等)を付けたくなった場合は、EnemyFactory.h
// のコメントと同様、Argsを増やすかEffectDefinition側にランダム幅の
// フィールドを足す形で拡張できる。
//
// database自体はFactory側で所有せず、const参照のまま各クロージャに
// 生ポインタでキャプチャしている(database不変・Factoryより長生きする
// 前提。EnemyFactoryと同じ制約)。
//
// 実際のコンポーネント組み立て(BuildEffect)は.cppに分離している。
// これにより、CreateEffect()を呼ぶだけの側は、VisualEffectComponent等
// エフェクト固有コンポーネントの実装詳細をincludeしなくて済む。
// ============================================================
class EffectFactory {
public:
	// databaseは所有しない(この後変更されない前提)。
	// 呼び出し側がdatabaseの寿命をこのFactoryより長く保つこと。
	explicit EffectFactory(const std::unordered_map<std::string, EffectDefinition>& database);
	~EffectFactory() = default;

	// コピー・ムーブ禁止(EnemyFactoryと同じ理由:
	// 各クロージャがdatabase内エントリへの生ポインタをキャプチャしているため、
	// このFactoryを複製すると寿命の前提が崩れる)。
	EffectFactory(const EffectFactory&) = delete;
	EffectFactory& operator=(const EffectFactory&) = delete;

	// 登録済みのeffectIdからGameObjectを生成する。未登録ならnullptr。
	// rotationを省略した場合は単位回転(向き無し)で生成する。
	GameObject* CreateEffect(ObjectManager& objectManager, const std::string& effectId,
		const Math::Vector3& position, const Math::Quaternion& rotation = Math::Quaternion::Identity) {
		return registry_.Create(effectId, objectManager, position, rotation);
	}

	bool IsKnownEffect(const std::string& effectId) const {
		return registry_.IsRegistered(effectId);
	}

private:
	// 実際のコンポーネント組み立て。定義はEffectFactory.cpp側。
	static GameObject* BuildEffect(ObjectManager& objectManager, const EffectDefinition& def,
		const Math::Vector3& position, const Math::Quaternion& rotation);

	GameObjectFactory<Math::Vector3, Math::Quaternion> registry_;
};
