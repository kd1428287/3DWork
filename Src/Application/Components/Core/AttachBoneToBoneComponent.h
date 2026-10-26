#pragma once
#include "TransformComponent.h"
#include "../Graphics/Animation/SkeletonComponent.h"

// ============================================================
// AttachBoneToBoneComponent: 子の特定ボーンを、親ソケット(BoneSocketComponent等)に重ねる。
// AttachToSocketComponentが「子の原点」を親に合わせるのに対し、こちらは「子のボーン」を合わせる。
// 子自身のTransformを逆算して動かす。子のSkeletonComponentは同じGameObjectに付ける。
// ============================================================
class AttachBoneToBoneComponent : public ComponentBase {
public:
	explicit AttachBoneToBoneComponent(GameObject* owner, Handle<TransformComponent> socketHandle, std::string childBoneName)
		: ComponentBase(owner), socketHandle_(socketHandle), childBoneName_(std::move(childBoneName)) {}

	void Awake() override {
		selfTransform_ = GetOwner()->GetComponent<TransformComponent>();
		childSkeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
		// 持ち主のscaleに掛ける基準として、構築後の自身のscaleを控える
		if (selfTransform_ != nullptr) baseScale_ = selfTransform_->GetScale();
	}

	void PostUpdate(float deltaTime) override {
		if (selfTransform_ == nullptr || childSkeleton_ == nullptr) return;

		TransformComponent* socket = socketHandle_.Resolve();
		if (socket == nullptr) return;

		// 子のscaleを先に確定する(子ボーンの平行移動はこのscaleで拡大されるため)
		Math::Vector3 ownerScale = Math::Vector3::One;
		if (TransformComponent* source = scaleSourceHandle_.Resolve()) ownerScale = source->GetScale();
		const Math::Vector3 childScale = baseScale_ * ownerScale;

		// 子ボーンのモデル空間行列(子のTransformに依存しないので循環しない)
		Math::Matrix childBoneLocal;
		if (!childSkeleton_->TryGetBoneLocalMatrix(childBoneName_, childBoneLocal)) return;

		// 親ソケットの位置・回転(scaleは無視)にオフセットを掛けた目標姿勢
		Math::Vector3 socketScale, socketPos;
		Math::Quaternion socketRot;
		if (!socket->GetWorldMatrix().Decompose(socketScale, socketRot, socketPos)) return;

		const Math::Matrix target =
			Math::Matrix::CreateFromQuaternion(rotation_)
			* Math::Matrix::CreateTranslation(position_ * ownerScale)
			* Math::Matrix::CreateFromQuaternion(socketRot)
			* Math::Matrix::CreateTranslation(socketPos);

		// 子ボーンを子scale込みにして、回転+平行移動だけ取り出す
		Math::Vector3 boneScale, bonePos;
		Math::Quaternion boneRot;
		if (!(childBoneLocal * Math::Matrix::CreateScale(childScale)).Decompose(boneScale, boneRot, bonePos)) return;

		const Math::Matrix boneRigid =
			Math::Matrix::CreateFromQuaternion(boneRot) * Math::Matrix::CreateTranslation(bonePos);

		// boneRigid * ownerRT = target  →  ownerRT = inverse(boneRigid) * target
		Math::Vector3 dummyScale, ownerPos;
		Math::Quaternion ownerRot;
		if (!(boneRigid.Invert() * target).Decompose(dummyScale, ownerRot, ownerPos)) return;

		selfTransform_->SetScale(childScale);
		selfTransform_->SetRotation(ownerRot);
		selfTransform_->SetPosition(ownerPos);
	}

	Handle<TransformComponent>& GetSocketHandle() { return socketHandle_; }
	void SetSocketHandle(Handle<TransformComponent> socketHandle) { socketHandle_ = socketHandle; }
	const std::string& GetChildBoneName() const { return childBoneName_; }
	void SetChildBoneName(std::string name) { childBoneName_ = std::move(name); }
	void SetLocalPosition(Math::Vector3 position) { position_ = position; }
	void SetLocalRotation(Math::Quaternion rotation) { rotation_ = rotation; }
	void SetScaleSource(Handle<TransformComponent> source) { scaleSourceHandle_ = source; }

private:
	Handle<TransformComponent> socketHandle_;
	std::string childBoneName_;

	TransformComponent* selfTransform_ = nullptr;
	SkeletonComponent* childSkeleton_ = nullptr;

	Handle<TransformComponent> scaleSourceHandle_;
	Math::Vector3 baseScale_ = Math::Vector3::One;

	Math::Vector3 position_;
	Math::Quaternion rotation_ = Math::Quaternion::Identity;
};
