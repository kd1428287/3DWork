#pragma once

// player用マーカー
class PlayerComponent : public ComponentBase
{
public:
	explicit PlayerComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override {
		if (auto* ctx = GetOwner()->GetContext()) {
			ctx->objectManager->SetPlayer(Handle<GameObject>(GetOwner()));
		}
	}

	void OnDestroy() override {
		if (auto* ctx = GetOwner()->GetContext()) {
			if (ctx->player == Handle<GameObject>(GetOwner())) {
				ctx->objectManager->SetPlayer(Handle<GameObject>());
			}
		}
	}
};