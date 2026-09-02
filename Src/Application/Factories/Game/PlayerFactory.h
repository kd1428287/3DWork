#pragma once

#include <string>

#include "PlayerDefinition.h"

class GameObject;
class ObjectManager;
class SkeletonComponent;
class BoneSocketComponent;

// ============================================================
// プレイヤー（自機）の生成に特化したファクトリークラス。
// 「何を作るか」はPlayerDefinitionが持ち、このクラスは
// 「PlayerDefinition通りにGameObject/Componentを組み立てる」ことだけに責務を絞る。
// ============================================================
class PlayerFactory {
public:
	PlayerFactory() = default;
	~PlayerFactory() = default;

	// コピー・ムーブ禁止
	PlayerFactory(const PlayerFactory&) = delete;
	PlayerFactory& operator=(const PlayerFactory&) = delete;

	// definitionPath からPlayerDefinitionを読み込んで生成する版（呼び出し側の利便用）。
	// 読み込みに失敗した場合は nullptr を返す。
	GameObject* CreatePlayer(ObjectManager& objectManager, const std::string& definitionPath);

	// 既に読み込み済みのPlayerDefinitionから生成する版。
	// リロード・テスト・ツールからの直接生成等はこちらを使う。
	GameObject* CreatePlayer(ObjectManager& objectManager, const PlayerDefinition& definition);

	GameObject* CreateSocket(ObjectManager& objectManager, std::string objectID, Handle<SkeletonComponent>& handle);
	GameObject* CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle, const WeaponDefinition& weaponDefinition, const IKChainDefinition& ikChain);
};
