#pragma once

#include "../../Core/Handle.h"
#include "TransformComponent.h"

class AttachToSocketComponent : public ComponentBase {
public:
	explicit AttachToSocketComponent(GameObject* owner, Handle<TransformComponent> socketHandle)
		: ComponentBase(owner), socketHandle_(socketHandle) {}

	void Start() override {
		selfTransform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float deltaTime) override {
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
		if (socket->GetWorldMatrix().Decompose(scale, rotation, position)) {
			const Math::Vector3 rotatedOffset = Math::Vector3::Transform(position_, rotation);
			selfTransform_->SetPosition(position + rotatedOffset);
			selfTransform_->SetRotation(rotation_ * rotation);
		}
	}

	Handle<TransformComponent>& GetSocketHandle() { return socketHandle_; }
	void SetSocketHandle(Handle<TransformComponent> socketHandle) { socketHandle_ = socketHandle; }
	void SetLocalPositon(Math::Vector3 position) { position_ = position; }
	void SetLocalRotation(Math::Quaternion rotation) { rotation_ = rotation; }

private:
	Handle<TransformComponent> socketHandle_;
	TransformComponent* selfTransform_ = nullptr; 

	Math::Vector3 position_;
	Math::Quaternion rotation_;
};
