#pragma once
#include "../Transform/TransformComponent.h"
#include "CameraComponent.h"

// 描画前にカメラ情報をシェーダーに転送
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

		camera_->GetCamera().SetCameraMatrix(transform_->GetWorldMatrix());
		camera_->GetCamera().SetToShader();
	}

protected:
	TransformComponent* transform_ = nullptr;
	CameraComponent* camera_ = nullptr;
};