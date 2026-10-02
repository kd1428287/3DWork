#pragma once
#include "TransformComponent.h"

class AttachToSocketComponent : public ComponentBase {
public:
	explicit AttachToSocketComponent(GameObject* owner, Handle<TransformComponent> socketHandle)
		: ComponentBase(owner), socketHandle_(socketHandle) {}

	void Awake() override {
		selfTransform_ = GetOwner()->GetComponent<TransformComponent>();
		// 持ち主のscaleに掛ける基準として、構築後の自身のscaleを控える
		if (selfTransform_ != nullptr) baseScale_ = selfTransform_->GetScale();
	}

	void PostUpdate(float deltaTime) override {
		if (selfTransform_ == nullptr) {
			return; // 自分にTransformComponentが無ければ何もしない
		}

		TransformComponent* socket = socketHandle_.Resolve();
		if (socket == nullptr) {
			return;
		}

		Math::Vector3 position;
		Math::Quaternion rotation;
		Math::Vector3 scale;

		// 持ち主(親)のscaleを、基準scaleとオフセットに反映する
		Math::Vector3 ownerScale = Math::Vector3::One;
		if (TransformComponent* source = scaleSourceHandle_.Resolve()) ownerScale = source->GetScale();
		selfTransform_->SetScale(baseScale_ * ownerScale);

		if (socket->GetWorldMatrix().Decompose(scale, rotation, position)) {
			const Math::Vector3 rotatedOffset = Math::Vector3::Transform(position_ * ownerScale, rotation);
			selfTransform_->SetPosition(position + rotatedOffset);
			selfTransform_->SetRotation(rotation_ * rotation);
		}
	}

	Handle<TransformComponent>& GetSocketHandle() { return socketHandle_; }
	void SetSocketHandle(Handle<TransformComponent> socketHandle) { socketHandle_ = socketHandle; }
	void SetLocalPositon(Math::Vector3 position) { position_ = position; }
	void SetLocalRotation(Math::Quaternion rotation) { rotation_ = rotation; }
	void SetScaleSource(Handle<TransformComponent> source) { scaleSourceHandle_ = source; }

private:
	Handle<TransformComponent> socketHandle_;
	TransformComponent* selfTransform_ = nullptr;

	Handle<TransformComponent> scaleSourceHandle_;
	Math::Vector3 baseScale_ = Math::Vector3::One;

	Math::Vector3 position_;
	Math::Quaternion rotation_;
};