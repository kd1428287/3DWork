#pragma once
#include "CameraOrbitComponent.h"
#include "Application/Core/EventBus/Events/TimeScaleEvents.h"
#include "Application/Core/EventBus/EventBus.h"
#include "Application/Core/EventBus/Events/InputLockEvents.h"

class CameraInputComponent : public ComponentBase
{
public:
	explicit CameraInputComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override
	{
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();

		inputLockSub_ = GLOBALEVENT->SubscribeScoped<Events::InputLockChanged>(
			[this](const Events::InputLockChanged& e) { inputLocked_ = e.locked; });
	}

	void PreUpdate(float deltaTime) override
	{
		if (orbit_ != nullptr && deltaTime != 0) {
			// --- マウス視点回転 -----------------------------------------
			// ロック中は0を渡し続け、視点を動かさない
			const Math::Vector2 look = inputLocked_ ? Math::Vector2::Zero : KdInputManager::Instance().GetAxisState("Look");
			orbit_->SetLookDelta(look);
		}
	}

private:
	CameraOrbitComponent* orbit_ = nullptr;

	bool inputLocked_ = false;
	ScopedSubscriber inputLockSub_;
};