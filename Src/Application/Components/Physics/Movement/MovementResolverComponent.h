#pragma once

#include "MovementComponent.h"
#include "VelocityComponent.h"
#include "TweenMoveComponent.h"

// 「移動処理の解決」を担う唯一の窓口。内力(MovementComponent)・
// 外力(VelocityComponent)・演出移動(TweenMoveComponent)を合成して
// Transformの位置を確定させることに加え、当たり判定システム
// (CollisionResolver)から渡される衝突補正を受け取り、位置と外力の
// 双方に反映する。
class MovementResolverComponent : public ComponentBase
{
public:
	explicit MovementResolverComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movement_ = GetOwner()->GetComponent<MovementComponent>();
		velocity_ = GetOwner()->GetComponent<VelocityComponent>();

		assert(transform_ != nullptr && "[MovementResolverComponent] Not Found TransformComponent");
	}

	void Start() override
	{
		tween_ = GetOwner()->GetComponent<TweenMoveComponent>();
		if (tween_)
		{
			1 + 2;
		}
	}

	void Update(float deltaTime) override
	{
		if (!transform_) return;

		const float clampedDeltaTime = std::min(deltaTime, kMaxDeltaTime);

		if (movement_) movement_->FetchDesiredVelocity();
		if (tween_)    tween_->Advance(clampedDeltaTime);

		// --- Tweenが動作中なら、絶対位置で上書きして終わり ----------
		if (tween_ && tween_->IsActive())
		{
			transform_->SetPosition(tween_->GetCurrentPosition());

			// 外力の減衰だけは進めておく
			if (velocity_) velocity_->DampingUpdate(clampedDeltaTime);

			// Tween中は「速度」という概念が無いため、参照側には
			// 動いていない扱いで見せる。
			lastComposedVelocity_ = Math::Vector3::Zero;
			return;
		}

		// 内力 + 外力の合成
		const Math::Vector3 externalVelocity = velocity_ ? velocity_->GetVelocity() : Math::Vector3::Zero;
		const Math::Vector3 internalVelocity = movement_ ? movement_->GetVelocity() : Math::Vector3::Zero;

		Math::Vector3 composedVelocity = externalVelocity + internalVelocity;

		// 水平と垂直を分けてクランプする
		Math::Vector3 horizontal{ composedVelocity.x, 0.0f, composedVelocity.z };
		if (horizontal.LengthSquared() > kMaxHorizontalSpeed * kMaxHorizontalSpeed)
		{
			horizontal.Normalize();
			horizontal *= kMaxHorizontalSpeed;
			composedVelocity.x = horizontal.x;
			composedVelocity.z = horizontal.z;
		}

		composedVelocity.y = std::clamp(composedVelocity.y, -kMaxVerticalSpeed, kMaxVerticalSpeed);

		transform_->SetPosition(transform_->GetPosition() + composedVelocity * clampedDeltaTime);
		lastComposedVelocity_ = composedVelocity;

		// 外力の減衰
		if (velocity_) velocity_->DampingUpdate(clampedDeltaTime);
	}

	// 位置に補正を適用しつつ、補正方向と逆(=めり込みを助長する向き)を向いている外力の成分を打ち消す
	void ApplyCollisionCorrection(const Math::Vector3& delta)
	{
		if (!transform_) return;
		if (delta.LengthSquared() <= 1e-10f) return;

		transform_->SetPosition(transform_->GetPosition() + delta);

		/*if (velocity_) {
			Math::Vector3 normal = delta;
			normal.Normalize();

			Math::Vector3 continuous = velocity_->GetContinuousVelocity();
			const float alongNormal = continuous.Dot(normal);
			if (alongNormal < 0.0f) {
				continuous -= normal * alongNormal;
				velocity_->SetContinuousVelocity(continuous);
			}
		}*/
	}

	// 他システム(アニメーションのブレンド、デバッグ表示等)が
	// 「今フレーム実際に合成された速度」を読みたい場合用。
	Math::Vector3 GetComposedVelocity() const { return lastComposedVelocity_; }

private:
	TransformComponent* transform_ = nullptr;
	MovementComponent* movement_ = nullptr;   
	VelocityComponent* velocity_ = nullptr;  
	TweenMoveComponent* tween_ = nullptr;    

	// GetComposedVelocity()で外部に公開する、直近Update()での合成速度。
	Math::Vector3 lastComposedVelocity_ = Math::Vector3::Zero;

	static constexpr float kMaxDeltaTime = 1.0f / 30.0f;

	// 水平方向の最大速度
	static constexpr float kMaxHorizontalSpeed = 25.0f;

	// 垂直方向の最大速度
	static constexpr float kMaxVerticalSpeed = 30.0f;
};