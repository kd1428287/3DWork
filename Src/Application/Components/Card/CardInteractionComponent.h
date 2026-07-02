#pragma once

class CardInteractionComponent : public ComponentBase {
public:
	explicit CardInteractionComponent(GameObject* owner) : ComponentBase(owner) {}

	void OnHoverEnter() { /* 拡大表示など */ }
	void OnClick() { if (onClicked_) onClicked_(GetOwner()); }

	void SetOnClicked(std::function<void(GameObject*)> cb) { onClicked_ = std::move(cb); }

private:
	std::function<void(GameObject*)> onClicked_;
};