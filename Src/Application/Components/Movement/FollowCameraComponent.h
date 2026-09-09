#pragma once
#include"../Camera/CameraComponent.h"

class FollowCameraComponent : public ComponentBase
{
public:
	explicit FollowCameraComponent(GameObject* owner) :ComponentBase(owner) {};

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float deltaTime) override
	{
		CameraComponent* camera = GetOwner()->GetContext()->activeCamera;

		if (followPosEnable_)
		{
			transform_->SetPosition(camera->GetPosition() + offset_);
		}
		if (followRotEnable_)
		{
			//transform_->SetRotation(camera->G)
		}
	}

	void SetOffset(const Math::Vector3& offset) { offset_ = offset; }

	void SetFollowEnable(bool pos,bool rot)
	{
		followPosEnable_ = pos;
		followRotEnable_ = rot;
	}

private:
	bool followPosEnable_ = true;
	bool followRotEnable_ = false;

	Math::Vector3 offset_ = {};

	TransformComponent* transform_ = nullptr;
};