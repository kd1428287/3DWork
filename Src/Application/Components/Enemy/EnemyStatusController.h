#pragma once
#include "EnemyState.h"
#include "../Movement/IMovementSource.h"
#include "../Movement/MovementComponent.h"
#include "../Transform/TransformComponent.h"

// ============================================================
// EnemyStatusController
// 基準点(Start()時点の初期位置)から左右に往復するだけの、
// 最も単純なパトロールAI。PlayerStatusControllerと同じ軽量Stateパターン
// (IEnemyState)で構成している。
//
// PlayerInputComponentのような「外部から入力を注入されるIMovementSource」
// ではなく、AI自身が「今どちらに動きたいか」を決定するため、
// このコントローラ自身がIMovementSourceを実装し、Start()時点で
// MovementComponentへ直接セットする(以前のAIWanderComponentと
// 同じ考え方)。
//
// ※ IMovementSource::GetDesiredVelocity()はdeltaTime引数を取らない
//   前提で書いている(最新のPlayerInputComponent.hに合わせた)。
//   もし手元のIMovementSource.hがまだdeltaTime引数ありのままなら、
//   このファイルとMovementComponent側の呼び出しをどちらかに揃えること。
// ============================================================
class EnemyStatusController : public ComponentBase, public IMovementSource {
public:
	// patrolDistance: 基準点からどれだけ離れたら折り返すか
	explicit EnemyStatusController(GameObject* owner, float patrolDistance = 3.0f)
		: ComponentBase(owner), patrolDistance_(patrolDistance) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();

		if (transform_ != nullptr) {
			basePosition_ = transform_->GetPosition();
		}
		if (movementComponent_ != nullptr) {
			movementComponent_->SetMovementSource(this);
		}

		TransitionTo(&stateWalkRight_);
	}

	void Update(float deltaTime) override {
		if (currentState_ != nullptr) {
			currentState_->Update(this, deltaTime);
		}
	}

	// --- Stateから参照される情報 ------------------------------------------

	const Math::Vector3& GetBasePosition() const { return basePosition_; }
	float GetPatrolDistance() const { return patrolDistance_; }

	Math::Vector3 GetCurrentPosition() const {
		return transform_ ? transform_->GetPosition() : basePosition_;
	}

	// --- 状態遷移(Stateから呼ばれる) ---------------------------------------

	void ChangeStateToWalkRight() { TransitionTo(&stateWalkRight_); }
	void ChangeStateToWalkLeft() { TransitionTo(&stateWalkLeft_); }

	// --- IMovementSourceの実装 ---------------------------------------------
	// MovementComponentから毎フレーム問い合わせられる。
	Math::Vector3 GetDesiredVelocity() override { return desiredDirection_; }

	// Stateがこのフレームの移動方向を設定するために使う。
	void SetDesiredDirection(const Math::Vector3& dir) { desiredDirection_ = dir; }

private:
	void TransitionTo(IEnemyState* nextState) {
		if (currentState_ == nextState) return;
		if (currentState_ != nullptr) currentState_->Exit(this);
		currentState_ = nextState;
		currentState_->Enter(this);
	}

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;

	Math::Vector3 basePosition_{};
	float patrolDistance_;
	Math::Vector3 desiredDirection_{};

	// Stateインスタンス(メモリ断片化を防ぐため実体をメンバで持つ。
	// PlayerStatusControllerと同じ理由)。
	StateWalkRight stateWalkRight_;
	StateWalkLeft stateWalkLeft_;
	IEnemyState* currentState_ = nullptr;
};
