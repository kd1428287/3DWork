#pragma once
#include "../Transform/TransformComponent.h"

class CameraTarget :public ComponentBase
{
public:
	CameraTarget(GameObject* owner)
		: ComponentBase(owner) {}
	virtual ~CameraTarget() = default;

	Math::Vector3 GetPos() 
	{
		return GetOwner()->GetComponent<TransformComponent>()->position;
	};

	Math::Matrix GetMatrix()
	{
		auto owner = GetOwner()->GetComponent<TransformComponent>();
		return
			Math::Matrix::CreateScale(owner->scale) *
			Math::Matrix::CreateFromYawPitchRoll(owner->rotation) *
			Math::Matrix::CreateTranslation(owner->position);
	}
};