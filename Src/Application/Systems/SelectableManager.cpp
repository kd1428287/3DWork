#include "Framework/KdFramework.h"

#include "SelectableManager.h"
#include "Application/Components/Graphics/UI/UITransformComponent.h"
#include "Application/Components/Graphics/UI/Button/SelectableComponent.h"
#include "Application/main.h"

void SelectableManager::Register(SelectableComponent* sel)
{
	if (std::find(list_.begin(), list_.end(), sel) == list_.end()) list_.push_back(sel);
}

void SelectableManager::Unregister(SelectableComponent* sel)
{
	list_.erase(std::remove(list_.begin(), list_.end(), sel), list_.end());
	if (hovered_ == sel) hovered_ = nullptr;
	if (pressed_ == sel) pressed_ = nullptr;
}

void SelectableManager::RegisterAction(const std::string& id, ActionPublisher publisher)
{
	publishers_[id] = std::move(publisher);
}

void SelectableManager::UnregisterAction(const std::string& id)
{
	publishers_.erase(id);
}

void SelectableManager::Post(UITrigger trigger, const UIAction& action)
{
	queue_.push_back({ trigger, action });
}

void SelectableManager::Dispatch()
{
	// 配信中に積まれたものは次回のUpdateで処理する
	std::vector<Pending> pending;
	pending.swap(queue_);

	for (const auto& p : pending)
	{
		// 型付きイベントの発行処理が登録されていれば先に呼ぶ
		auto it = publishers_.find(p.action.id);
		if (it != publishers_.end())
		{
			ActionPublisher fn = it->second;
			fn(p.action.param);
		}

		GLOBALEVENT.Publish(UIActionEvent(p.trigger, p.action.id, p.action.param));
	}
}

void SelectableManager::SetActiveGroup(int group)
{
	if (activeGroup_ == group) return;
	activeGroup_ = group;
	ClearHoverAndPress();

	// 対象外になった選択中のUIは解除する
	SelectableComponent* cur = SelectableComponent::GetCurrent();
	if (cur && !IsActive(cur)) cur->Deselect();
}

void SelectableManager::SetButtonNames(const std::string& confirm, const std::string& up, const std::string& down,
	const std::string& left, const std::string& right)
{
	confirmName_ = confirm;
	upName_ = up;
	downName_ = down;
	leftName_ = left;
	rightName_ = right;
}

bool SelectableManager::IsActive(const SelectableComponent* sel) const
{
	return sel->IsInteractable() && (activeGroup_ < 0 || sel->GetGroup() == activeGroup_);
}

SelectableComponent* SelectableManager::FindHit(const Math::Vector2& mouse) const
{
	SelectableComponent* best = nullptr;
	for (auto* sel : list_)
	{
		if (!IsActive(sel) || !sel->HitTest(mouse)) continue;
		// 同じlayerなら後に登録されたものを手前とみなす
		if (!best || sel->GetLayer() >= best->GetLayer()) best = sel;
	}
	return best;
}

SelectableComponent* SelectableManager::FindFirstActive() const
{
	for (auto* sel : list_)
	{
		if (IsActive(sel)) return sel;
	}
	return nullptr;
}

void SelectableManager::SetHover(SelectableComponent* next)
{
	if (hovered_ == next) return;

	if (hovered_) hovered_->SetHovered(false);
	hovered_ = next;
	if (hovered_)
	{
		hovered_->SetHovered(true);
		if (hoverSelects_) hovered_->Select();
	}
}

void SelectableManager::ClearHoverAndPress()
{
	if (hovered_) hovered_->SetHovered(false);
	if (pressed_) pressed_->Release(false);
	hovered_ = nullptr;
	pressed_ = nullptr;
}

void SelectableManager::Update()
{
	// 非アクティブ時はGetAsyncKeyStateが反応してしまうため何もしない
	HWND hwnd = Application::Instance().GetWindowHandle();
	if (!hwnd || GetForegroundWindow() != hwnd)
	{
		ClearHoverAndPress();
		hasPrevMouse_ = false;
		Dispatch();
		return;
	}

	auto& input = KdInputManager::Instance();

	Math::Vector2 mouse;
	const bool hasMouse = UITransformComponent::GetMousePos(mouse);

	// マウスが動いたらマウス操作へ切り替える
	if (hasMouse && hasPrevMouse_ && mouse != prevMouse_) mouseMode_ = true;
	hasPrevMouse_ = hasMouse;
	if (hasMouse) prevMouse_ = mouse;

	// 方向入力が押されたらキー操作へ切り替え、選択を移動する
	const std::pair<const std::string*, SelectableComponent::Direction> dirs[] = {
		{ &upName_,		SelectableComponent::Direction::Up },
		{ &downName_,	SelectableComponent::Direction::Down },
		{ &leftName_,	SelectableComponent::Direction::Left },
		{ &rightName_,	SelectableComponent::Direction::Right },
	};
	for (const auto& d : dirs)
	{
		if (!input.IsPress(*d.first)) continue;

		mouseMode_ = false;

		SelectableComponent* cur = SelectableComponent::GetCurrent();
		if (cur && IsActive(cur))
		{
			cur->Navigate(d.second);
		}
		else if (SelectableComponent* first = FindFirstActive())
		{
			// 未選択なら最初の1つを選ぶ
			first->Select();
		}
		break;
	}

	// ホバーはマウス操作中のみ。キー操作中はカーソル下でも反応させない
	SetHover(mouseMode_ && hasMouse ? FindHit(mouse) : nullptr);

	// 決定。マウス操作ならホバー中のUI、キー操作なら選択中のUIが対象
	if (input.IsPress(confirmName_))
	{
		SelectableComponent* target = mouseMode_ ? hovered_ : SelectableComponent::GetCurrent();
		if (target && IsActive(target))
		{
			pressed_ = target;
			target->Press();
		}
	}

	// 離した時にマウスが押したUIの上にいなければ取り消し
	if (input.IsRelease(confirmName_) && pressed_)
	{
		SelectableComponent* target = pressed_;
		pressed_ = nullptr;
		target->Release(!mouseMode_ || hovered_ == target);
	}

	Dispatch();
}