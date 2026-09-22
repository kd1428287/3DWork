#pragma once
#include"../../GamePlay/Camera/CameraComponent.h"
#include "../../Tags/IRenderable.h"

// 初期設定(Prefab/JSONから渡す値)。
struct FollowCameraConfig
{
	Math::Vector3 offset = {};
	bool followPosition = true;
	bool followRotation = false;
};

// PreDraw()を使うためIRenderable継承
class FollowCameraComponent : public ComponentBase, public IRenderable
{
public:
	using Config = FollowCameraConfig;

	explicit FollowCameraComponent(GameObject* owner) :ComponentBase(owner) {};

	void SetConfig(const Config& config)
	{
		offset_ = config.offset;
		SetFollowEnable(config.followPosition, config.followRotation);
	}

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void PreDraw() override
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
	bool followPosEnable_ = FollowCameraConfig{}.followPosition;
	bool followRotEnable_ = FollowCameraConfig{}.followRotation;

	Math::Vector3 offset_ = FollowCameraConfig{}.offset;

	TransformComponent* transform_ = nullptr;
};