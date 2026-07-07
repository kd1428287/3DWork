#pragma once
#include <string>
#include <unordered_map>

#include "../../Components/Player/PlayerInputComponent.h"
#include "../../Components/Transform/MovementComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../GameObjectFactory.h"

class PlayerFactory {
public:
	// カード定義データベースを受け取り、登録できるものを全て登録しておく。
	// database自体は所有しない(この後変更されない前提)。
	explicit PlayerFactory() {
		registry_.Register(0, [](ObjectManager& objectManager, int ownerPlayerId) {
			GameObject* player = objectManager.Instantiate("player");
			player->AddComponent<TransformComponent>();
			player->AddComponent<MovementComponent>();
			player->AddComponent<CameraTargetComponent>();
			// 必要に応じてここでまとめて付与していく
			return player;
			});
	}

	GameObject* CreatePlayer(ObjectManager& objectManager, const std::string& PlayerId, int ownerPlayerId) {
		return registry_.Create(PlayerId, objectManager, ownerPlayerId);
	}

	bool IsKnownPlayer(const std::string& PlayerId) const {
		return registry_.IsRegistered(PlayerId);
	}

private:
	GameObjectFactory<int> registry_;
};