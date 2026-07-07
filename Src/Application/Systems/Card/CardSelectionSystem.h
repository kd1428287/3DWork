#pragma once
#include "../../Components/Card/CardSelectionComponent.h"
#include "HandSystem.h"
// ============================================================
// カード選択システム。
//
// 「1キーが押されたら、"今" 手札の1番目にいるカードをアクティブにする」
// という処理の要点は、キー入力の時点では対象がまだ決まっておらず、
// Handという「並び順の正本」に都度問い合わせて初めて対象が確定する点。
//
// そのため、あらかじめ特定のComponentを登録しておくPush方式ではなく、
// 「入力が来た瞬間にHandへ問い合わせて解決する」方式にしている。
// Hand自体はSceneに1つだけの既知の対象なので、直接参照で持てる
// (カードのように無数にある対象ではない)。
//
// 選択結果(CardSelectedEvent)はイベントバスで発行する。
// 「誰がこの選択に興味を持つか」(UIハイライト、効果対象決定、
// 演出システムなど)はCardSelectionSystm側では関知しないため。
// ============================================================
class CardSelectionSystem {
public:
	CardSelectionSystem(HandSystem& hand, EventBus& bus) : hand_(hand), bus_(bus) {}

	void Update(float /*deltaTime*/) {
		auto& input = KdInputManager::Instance();
		std::string buff;
		for (int i = 0; i < 10; i++)
		{
			buff = std::to_string(i) + "key";
			if (input.IsPress(buff))SelectByIndex(i);
		}
	}

	void SelectByIndex(std::size_t index) {
		GameObject* card = hand_.GetCardAt(index);
		if (card == nullptr) return;  // 手札がその数に満たない場合は何もしない

		auto* selection = card->GetComponent<CardSelectionComponent>();
		if (selection == nullptr) return;  // 選択コンポーネントを持たないカードは対象外
		if (selection->IsActive()) return;  // 既にアクティブなら何もしない(連続Publish防止)

		GameObject* previous = ClearCurrentActive();

		selection->SetActive(true);
		currentActive_ = card;

		auto event = Events::Card::CardSelectedEvent();
		event.card = card;
		event.previous = previous;
		bus_.publish(event);
	}

private:
	// 現在アクティブなカードの選択状態を解除し、そのGameObjectを返す
	GameObject* ClearCurrentActive() {
		if (currentActive_ == nullptr) return nullptr;
		if (auto* prevSelection = currentActive_->GetComponent<CardSelectionComponent>()) {
			prevSelection->SetActive(false);
		}
		GameObject* previous = currentActive_;
		currentActive_ = nullptr;
		return previous;
	}

	HandSystem& hand_;
	EventBus& bus_;
	GameObject* currentActive_ = nullptr;
};