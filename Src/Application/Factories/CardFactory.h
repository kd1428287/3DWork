#pragma once
#include <string>
#include <unordered_map>

#include "../Components/Card/CardDefinition.h"
#include "../Components/Card/CardIdentityComponent.h"
#include "../Components/Card/CardSelectionComponent.h"
#include "../Components/Card/CardInteractionComponent.h"
#include "../Components/Card/CardRenderComponent.h"
#include "../Components/Card/CardStateComponent.h"
#include "GameObjectFactory.h"

// ============================================================
// カード生成のFactory。
//
// 中身は汎用の GameObjectFactory<int>(int = ownerPlayerId) を
// そのまま使っているだけ。CardFactory自体が持つ固有の知識は
// 「カード定義データベースから生成関数を1つずつ登録する」部分のみで、
// 登録・検索・生成の仕組み自体は再発明していない。
//
// 外から見えるAPI(CreateCard)は変えていないため、既存の呼び出し側
// (card_factory_demo.cpp等)は無変更のまま動作する。
// ============================================================
class CardFactory {
public:
	// カード定義データベースを受け取り、登録できるものを全て登録しておく。
	// database自体は所有しない(この後変更されない前提)。
	explicit CardFactory(const std::unordered_map<std::string, CardDefinition>& database) {
		for (const auto& [id, def] : database) {
			const CardDefinition* defPtr = &def;  // database不変前提で安全に保持できる
			registry_.Register(id, [defPtr](ObjectManager& objectManager, int ownerPlayerId) {
				GameObject* card = objectManager.Instantiate(defPtr->name);
				card->AddComponent<CardIdentityComponent>(defPtr, ownerPlayerId);
				card->AddComponent<CardSelectionComponent>();
				card->AddComponent<CardInteractionComponent>();
				card->AddComponent<CardRenderComponent>();	//ファイルパス
				card->AddComponent<CardStateComponent>();
				card->AddComponent<TransformComponent>();
				// 必要に応じてここでまとめて付与していく
				return card;
				});
		}
	}

	GameObject* CreateCard(ObjectManager& objectManager, const std::string& cardId, int ownerPlayerId) {
		return registry_.Create(cardId, objectManager, ownerPlayerId);
	}

	bool IsKnownCard(const std::string& cardId) const {
		return registry_.IsRegistered(cardId);
	}

private:
	GameObjectFactory<int> registry_;
};