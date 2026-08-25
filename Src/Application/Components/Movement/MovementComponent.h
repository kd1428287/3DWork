#pragma once
#include <cstdio>

#include "IMovementSource.h"
#include "../Transform/TransformComponent.h"

// ============================================================
// 「動く」という処理自体(Transformを書き換える部分)と、
// 「どちらに動くか決める」処理(IMovementSource)を分離している。
// これにより、手動操作(入力コンポーネント)・自動操作(AIコンポーネント)を
// 同じMovementComponentでそのまま扱える。
//
// 依存コンポーネントの取得は Start() で行うのが定石
// (Awake順序に依らず、全コンポーネントの追加が終わってから解決できるため)。
//
// 「今、入力移動を許可してよいか」の判断はここでは持たない。
// 以前はVelocityComponent::IsImpulseActive()を自前で監視し、ノックバック中は
// 位置更新を一時停止していたが、Player/Enemy双方のStatusControllerが
// 各State(Stagger/StateKnockback等)側で既に「いつ移動を再開してよいか」を
// 判断してComponentBaseのEnabled(ComponentBase::SetEnabled)で
// 制御しているため、ここでさらにノックバックの残量を見て止めてしまうと、
// 「Stateとしてはもう移動可能」なのに動けない、という二重管理の不整合が
// 起きる(Playerがスタン明け後もノックバックの減衰待ちで動けなくなる不具合の
// 原因だった)。よって移動を止めるかどうかは呼び出し側(各StatusController)の
// 責任とし、MovementComponent自体はEnabled=trueである限り常に入力移動を
// 適用する。ノックバックによる移動はVelocityComponent側が別途Translateする
// ため、両者が同時に有効でも単純に加算されるだけで競合はしない。
// ============================================================
class MovementComponent : public ComponentBase {
public:
	explicit MovementComponent(GameObject* owner, float speed = 1.0f)
		: ComponentBase(owner), speed_(speed) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr || source_ == nullptr) return;

		const Math::Vector3 v = source_->GetDesiredVelocity();
		transform_->Translate(v * (speed_ * deltaTime));
	}

	// 動きの決定方法(手動入力 / AI など)を差し替える。
	// PlayerInputComponent* や AIWanderComponent* など、
	// IMovementSourceを実装したコンポーネントを渡す。
	void SetMovementSource(IMovementSource* source) { source_ = source; }

	void SetSpeed(float speed) { speed_ = speed; }
	float GetSpeed() const { return speed_; }

private:
	float speed_;
	TransformComponent* transform_ = nullptr;
	IMovementSource* source_ = nullptr;
};