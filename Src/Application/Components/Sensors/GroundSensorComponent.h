#pragma once
#include <cstdio>

#include "../Collision/ColliderComponent.h"
#include "../Collision/CharacterCollisionDefaults.h"
#include "../Transform/TransformComponent.h"
#include "../../Core/GameObject.h"
#include "../../Core/SceneContext.h"

// 足元にレイを飛ばして接地判定だけを行う
class GroundSensorComponent : public ComponentBase {
public:
	explicit GroundSensorComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ == nullptr) {
			std::printf(
				"[GroundSensorComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	void PostUpdate(float /*deltaTime*/) override {
		if (transform_ == nullptr) return;

		const SceneContext* context = GetOwner()->GetContext();
		const ColliderRegistry* registry = context ? context->colliderRegistry : nullptr;
		if (registry == nullptr) return;

		const Math::Vector3 origin = transform_->GetPosition() + Math::Vector3(0.0f, -footOffset, 0.0f);

		RaycastSystem::Hit hit;
		const bool found = RaycastSystem::RaycastClosest(
			*registry, origin, Math::Vector3::Down, checkDistance,
			ColliderCategory::Ground, hit);

		wasGroundedLastQuery_ = isGrounded_;

		isGrounded_ = found;
		groundNormal_ = found ? hit.result.hitNormal : Math::Vector3::Up;
		groundObject_ = found ? hit.object : nullptr;
	}

	// --- 参照 ----------------------------------------------------

	bool IsGrounded() const { return isGrounded_; }

	// 今フレームで新たに接地した(直前フレームは非接地だった)
	bool JustLanded() const { return isGrounded_ && !wasGroundedLastQuery_; }

	// 今フレームで新たに接地を離れた(直前フレームは接地していた)
	bool JustLeftGround() const { return !isGrounded_ && wasGroundedLastQuery_; }

	// 接地している地面の法線(非接地時はMath::Vector3::Upを返す)
	const Math::Vector3& GetGroundNormal() const { return groundNormal_; }

	// 接地している相手のGameObject(非接地時はnullptr)
	GameObject* GetGroundObject() const { return groundObject_; }

	// --- パラメータ ------------------------------------------------

	// TransformComponentの位置(通常はキャラの中心と想定)から、
	// どれだけ下に足裏があるか。既定値はCharacterCollisionDefaults::
	// kFootOffsetを参照する(Bodyコライダーの下端の高さと必ず一致させる
	// 必要があるため。詳細はCharacterCollisionDefaults.h参照)。
	float footOffset = 0;// CharacterCollisionDefaults::kFootOffset;

	// 足裏からどれだけ下まで地面を探すか。
	// 0に近すぎると段差や坂でわずかに浮いた瞬間に非接地判定されてしまい、
	// 大きすぎると宙に浮いていても接地扱いになるため、キャラの
	// 移動速度・段差の高さに応じて調整すること。
	float checkDistance = 0.15f;

private:
	TransformComponent* transform_ = nullptr;

	bool isGrounded_ = false;
	bool wasGroundedLastQuery_ = false;
	Math::Vector3 groundNormal_ = Math::Vector3::Up;
	GameObject* groundObject_ = nullptr;
};