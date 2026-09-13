#pragma once
#include "CameraOrbitComponent.h"

class CameraInputComponent : public ComponentBase
{
public:
	explicit CameraInputComponent(GameObject* owner) :ComponentBase(owner) {}

	void Start()override
	{
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();
	}

	void PreUpdate(float deltaTime) override
	{
		if (orbit_ != nullptr) {
			// --- マウス視点回転 -----------------------------------------
			const Math::Vector2 look = KdInputManager::Instance().GetAxisState("Look");
			orbit_->SetLookDelta(look);
		}
	}

private:
	CameraOrbitComponent* orbit_ = nullptr;
};