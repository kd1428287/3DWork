#pragma once

enum class CardZone { Deck, Hand, Field, Graveyard };

class CardStateComponent : public ComponentBase {
public:
	explicit CardStateComponent(GameObject* owner) : ComponentBase(owner) {}

	CardZone GetZone() const { return zone_; }
	void SetZone(CardZone zone) { zone_ = zone; }

	bool IsTapped() const { return tapped_; }
	void SetTapped(bool tapped) { tapped_ = tapped; }

	// バフ等で変動する実行時コスト・パワー等はここで持つ
	int GetCurrentCost() const { return currentCost_; }
	void SetCurrentCost(int cost) { currentCost_ = cost; }

private:
	CardZone zone_ = CardZone::Deck;
	bool tapped_ = false;
	int currentCost_ = 0;
};