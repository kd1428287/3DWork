#pragma once
#include "CardDefinition.h"

class CardIdentityComponent : public ComponentBase {
public:
	CardIdentityComponent(GameObject* owner, const CardDefinition* def, int ownerPlayerId)
		: ComponentBase(owner), definition_(def), ownerPlayerId_(ownerPlayerId) {}

	const CardDefinition* GetDefinition() const { return definition_; }
	int GetOwnerPlayerId() const { return ownerPlayerId_; }

private:
	const CardDefinition* definition_;  // マスターデータへの非所有参照
	int ownerPlayerId_;
};