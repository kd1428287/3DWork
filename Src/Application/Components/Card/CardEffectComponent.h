#pragma once
#include "CardIdentityComponent.h"

class CardEffectComponent : public ComponentBase {
public:
	explicit CardEffectComponent(GameObject* owner) : ComponentBase(owner) {}

	void OnDraw() { Trigger("OnDraw"); }
	void OnPlay() { Trigger("OnPlay"); }
	void OnDeath() { Trigger("OnDeath"); }

private:
	void Trigger(const std::string& eventName) {
		auto* identity = GetOwner()->GetComponent<CardIdentityComponent>();
		if (!identity) return;
		// effectScript(識別子)を元に、外部の効果解決システムへ委譲
		//EffectResolver::Instance().Resolve(identity->GetDefinition()->effectScript, eventName, GetOwner());
	}
};