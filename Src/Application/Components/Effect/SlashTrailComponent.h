#pragma once
#include "../Transform/TransformComponent.h"
#include "../../Effect/SlashTrailEvents.h"

class SlashTrailComponent : public ComponentBase {
public:
	explicit SlashTrailComponent(GameObject* owner, const std::string& trailName = "")
		: ComponentBase(owner), trailName_(trailName)
	{
	}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float dt)override
	{
		if (!emitting_ || transform_ == nullptr)return;

		// ローカル座標のTip/Baseをワールド座標に変換してから送る
		const Math::Matrix& worldMat = transform_->GetWorldMatrix();
		Math::Vector3 worldTip = Math::Vector3::Transform(tip_, worldMat);
		Math::Vector3 worldBase = Math::Vector3::Transform(base_, worldMat);

		PublishSlashTrailPositionUpdate(
			*GetOwner()->GetContext()->eventBus, instanceKey_, worldTip, worldBase);
	}

	void SetBaseTip(Math::Vector3 base, Math::Vector3 tip)
	{
		base_ = base;
		tip_ = tip;
	}

	void SetKey(std::string key) { instanceKey_ = key; }

	void StartEmit()
	{
		if (emitting_) return;
		emitting_ = true;
		PublishSlashTrailBegin(*GetOwner()->GetContext()->eventBus, instanceKey_, trailName_);
	}

	void StopEmit()
	{
		if (!emitting_) return;
		emitting_ = false;
		PublishSlashTrailEnd(*GetOwner()->GetContext()->eventBus, instanceKey_);
	}

private:
	TransformComponent* transform_ = nullptr;

	std::string instanceKey_ = "";
	std::string trailName_ = "";
	Math::Vector3 base_{};
	Math::Vector3 tip_{};

	bool emitting_ = false;
};