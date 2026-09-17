#pragma once
#include "CameraOrbitComponent.h"
#include "Application/Core/EventBus/Events/TimeScaleEvents.h"

class CameraInputComponent : public ComponentBase
{
public:
	explicit CameraInputComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override
	{
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();
	}

	void PreUpdate(float deltaTime) override
	{
		if (orbit_ != nullptr && deltaTime != 0) {
			// --- マウス視点回転 -----------------------------------------
			const Math::Vector2 look = KdInputManager::Instance().GetAxisState("Look");
			orbit_->SetLookDelta(look);
		}
	}

private:
	CameraOrbitComponent* orbit_ = nullptr;
};