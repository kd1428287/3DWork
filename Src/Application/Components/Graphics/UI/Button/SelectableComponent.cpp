#include "Framework/KdFramework.h"

#include "SelectableComponent.h"
#include "Application/Systems/SelectableManager.h"
#include "Application/Components/Graphics/UI/UITransformComponent.h"

void SelectableComponent::Awake()
{
	ui_ = GetOwner()->GetComponent<UITransformComponent>();
	SelectableManager::Instance().Register(this);
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
