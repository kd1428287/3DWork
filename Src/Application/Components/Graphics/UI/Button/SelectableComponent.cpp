#include "Framework/KdFramework.h"

#include "SelectableComponent.h"
#include "Application/Systems/SelectableManager.h"
#include "Application/Components/Graphics/UI/UITransformComponent.h"

void SelectableComponent::Awake()
{
	ui_ = GetOwner()->GetComponent<UITransformComponent>();
	SelectableManager::Instance().Register(this);
}

void SelectableComponent::Update(float)
{
	if (!ui_ || hoverScale_ == 1.0f) return;

	// 元の拡大率は初回に控える。演出はヒットストップ等に影響されない時間で進める
	if (!baseCaptured_) { baseScale_ = ui_->GetScale(); baseCaptured_ = true; }
	const float dt = GetOwner()->GetContext()->unscaledDeltaTime;

	const float target = hovered_ ? hoverScale_ : 1.0f;
	scaleRate_ += (target - scaleRate_) * (1.0f - std::exp(-hoverSpeed_ * dt));
	ui_->SetScale(baseScale_ * scaleRate_);
}

bool SelectableComponent::HitTest(const Math::Vector2& point) const
{
	if (!ui_ || !interactable_) return false;

	Math::Vector2 min, size;
	if (!ui_->ResolveRect(min, size)) return false;

	return point.x >= min.x && point.x <= min.x + size.x
		&& point.y >= min.y && point.y <= min.y + size.y;
}

SelectableComponent::~SelectableComponent()
{
	if (s_current == this) s_current = nullptr;
	SelectableManager::Instance().Unregister(this);
}