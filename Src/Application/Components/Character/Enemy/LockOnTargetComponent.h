#pragma once
#include "../../Transform/TransformComponent.h"
#include "../../Animation/BoneSocketComponent.h"
#include "../Data/HealthComponent.h"

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

	void Start() override {
		// ロック照準を合わせたい高さがあるならBoneSocketComponentを
		// 併用する想定(無ければ足元のTransform原点にフォールバック)。
		reticleSocket_ = GetOwner()->GetComponent<BoneSocketComponent>();

		// HealthComponentのDiedEvent購読で死亡時に自動解除する。
		// EnemyStatusController側に「死んだらロック解除して」という
		// 依頼コードを書かせずに済む(HealthComponentはPlayer/Enemy共有のため
		// この購読だけで両対応できる)。
		if (HealthComponent* health = GetOwner()->GetComponent<HealthComponent>()) {
			EventBus& localBus = GetOwner()->GetLocalEventBus();
			const SubscriptionId id = localBus.Subscribe<HealthComponent::DiedEvent>(
				[this](const HealthComponent::DiedEvent&) { isLockable_ = false; });
			subscriber_ = ScopedSubscriber(&localBus, id);
		}

		// ※ 自己登録用のレジストリは持たない。ObjectManager::FindComponents<T>()
		// が呼ばれるたびに現存するものだけを拾ってくれるため不要
		// (PlayerLockOnComponent::FindNearestToScreenCenter()参照)。
	}

	bool IsLockable() const { return isLockable_; }

	// ロック時にカメラ/UIが狙う実座標。ソケットがあればそちらを優先し、
	// 無ければTransform原点(足元)にフォールバックする。
	Math::Vector3 GetReticlePosition() const {
		if (reticleSocket_ != nullptr) return reticleSocket_->GetPosition();
		TransformComponent* transform = GetOwner()->GetComponent<TransformComponent>();
		return transform ? transform->GetPosition() : Math::Vector3::Zero;
	}

private:
	bool isLockable_ = true;
	BoneSocketComponent* reticleSocket_ = nullptr;
	ScopedSubscriber subscriber_;
};