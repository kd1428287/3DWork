#pragma once
#include "../../../Core/BoneSocketComponent.h"
#include "Application/Core/EventBus/Events/CombatEvents.h"
#include "../Common/HealthComponent.h"

// ============================================================
// 「ロックオン可能な対象である」ことを示すマーカー兼データコンポーネント。
// PlayerLockOnComponentはEnemyStatusControllerを直接知らず、
// このコンポーネントの有無とIsLockable()だけを見て判定する。
// 敵以外(破壊オブジェクト等)へロックオンを拡張したい場合も、
// このコンポーネントを付けるだけで対応できるようにする狙い。
// ============================================================
class LockOnTargetComponent : public ComponentBase
{
public:
	explicit LockOnTargetComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override {
		// ロック照準を合わせたい高さがあるならBoneSocketComponentを併用する
		reticleSocket_ = GetOwner()->GetComponent<BoneSocketComponent>();

		// HealthComponentのDiedEvent購読で死亡時に自動解除する
		if (GetOwner()->HasComponent<HealthComponent>()) {
			subscriber_ = GetOwner()->GetLocalEventBus().SubscribeScoped<Events::Died>(
				[this](const Events::Died &e) { isLockable_ = false; });
		}
	}

	bool IsLockable() const { return isLockable_; }

	void SetOffset(const Math::Vector3& offset) { offset_ = offset; }

	// ロック時にカメラ/UIが狙う実座標
	Math::Vector3 GetReticlePosition() const {
		if (reticleSocket_ != nullptr) return reticleSocket_->GetPosition();
		TransformComponent* transform = GetOwner()->GetComponent<TransformComponent>();
		return transform ? transform->GetPosition() + offset_ : Math::Vector3::Zero;
	}

private:
	bool isLockable_ = true;
	Math::Vector3 offset_ = { 0,1.5,0 };
	BoneSocketComponent* reticleSocket_ = nullptr;
	ScopedSubscriber subscriber_;
};