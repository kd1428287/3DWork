#pragma once

#include "../../Tags/IMovementSource.h"

// 内力
class MovementComponent : public ComponentBase {
public:
	explicit MovementComponent(GameObject* owner, float speed = 1.0f)
		: ComponentBase(owner), speed_(speed) {}

	void Start() override
	{
		// 自オブジェクトに入力ソースがあるならそちらを優先
		auto sources = GetOwner()->GetTagged<IMovementSource>();
		assert(sources.size() <= 1 && "MovementComponent: 複数のIMovementSourceが見つかりました");
		if (!sources.empty())
		{
			source_ = sources.front();
		};
	}

	void AcumulateDesiredVelocity()
	{
		if (!source_)return;

		desiredVelocity_ = source_->GetDesiredVelocity();
	}

	// 動きの決定方法(手動入力 / AI など)を差し替える。
	void SetMovementSource(IMovementSource* source) { source_ = source; }

	void SetSpeed(float speed) { speed_ = speed; }
	float GetSpeed() const { return speed_; }

	Math::Vector3 GetVelocity() const { return desiredVelocity_ * speed_; }

private:
	float speed_;
	Math::Vector3 desiredVelocity_;
	IMovementSource* source_ = nullptr;
};