#pragma once
#include "../Transform/TransformComponent.h"
#include "CameraComponent.h"

class CameraViewComponent : public ComponentBase, public IRenderable
{
public:
	explicit CameraViewComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		camera_ = GetOwner()->GetComponent<CameraComponent>();
	}

	void PreDraw()override
	{
		if (!camera_ || !transform_) { return; }

		//camera_->GetCamera().SetCameraMatrix(transform_->matrix);
		camera_->GetCamera().SetCameraMatrix(Math::Matrix::Identity);
		camera_->GetCamera().SetToShader();
	}

protected:
	TransformComponent* transform_ = nullptr;
	CameraComponent* camera_ = nullptr;
};