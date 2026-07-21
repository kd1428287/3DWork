#pragma once
#include "../../Core/Handle.h"
#include "../Transform/TransformComponent.h"
#include "SkeletonComponent.h"

// ============================================================
// BoneSocketComponent: SocketComponentのボーン版。
// 親がTransformComponentではなく「SkeletonComponent上の特定ボーン」
// である点だけが異なる。それ以外(誰かを追従させる仕事は持たない、
// AttachToSocketComponentから読まれるだけ)はSocketComponentと同一。
//
// TransformComponentを継承しているため、AttachToSocketComponentは
// 無改造のままこれをHandle<TransformComponent>として扱える。
// ============================================================
class BoneSocketComponent : public TransformComponent {
public:
	explicit BoneSocketComponent(GameObject* owner, Handle<SkeletonComponent> skeletonHandle, std::string boneName)
		: TransformComponent(owner), skeletonHandle_(skeletonHandle), boneName_(std::move(boneName)) {}

	Handle<SkeletonComponent>& GetSkeletonHandle() { return skeletonHandle_; }
	const std::string& GetBoneName() const { return boneName_; }

protected:
	uint32_t GetVersion() const override {
		SkeletonComponent* skeleton = skeletonHandle_.Resolve();
		return localVersion_ + (skeleton ? skeleton->GetBoneVersion() : 0);
	}

	void RefreshCacheIfNeeded() const override {
		const uint32_t currentVersion = GetVersion();
		if (currentVersion == cachedVersion_) { return; }

		Math::Matrix boneWorld = Math::Matrix::Identity;
		SkeletonComponent* skeleton = skeletonHandle_.Resolve();
		if (skeleton != nullptr)
		{
			skeleton->TryGetBoneWorldMatrix(boneName_, boneWorld);
		}

		cachedUnscaledMatrix_ =
			Math::Matrix::CreateFromQuaternion(rotation_)
			* Math::Matrix::CreateTranslation(position_)
			* boneWorld;

		cachedWorldMatrix_ =
			Math::Matrix::CreateScale(scale_)
			* Math::Matrix::CreateFromQuaternion(rotation_)
			* Math::Matrix::CreateTranslation(position_)
			* boneWorld;

		cachedVersion_ = currentVersion;
	}

private:
	Handle<SkeletonComponent> skeletonHandle_;
	std::string boneName_;
};