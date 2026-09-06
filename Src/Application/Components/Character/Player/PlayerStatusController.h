#pragma once

#include <array>
#include <cmath>
#include <string>
#include "PlayerCombatTypes.h"
#include "PlayerCombatDataTable.h"
#include "PlayerInputComponent.h"
#include "PlayerLockOnComponent.h"
#include "PlayerMovementAnimationComponent.h"
#include "../../Movement/MovementComponent.h" 
#include "../../Movement/FacingDirectionComponent.h" 
#include "../../Transform/TransformComponent.h"
#include "PlayerState.h"
#include "../StateMachine/StateMachine.h"
#include "../../Animation/ModelAnimatorComponent.h"
#include "../../Collision/ColliderComponent.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../../Effect/TrailPolygonComponent.h" 
#include "../../../Core/Handle.h"
#include "../Data/IHitReactionQuery.h"
#include "../Data/HitReactionComponent.h"

class PlayerStatusController : public ComponentBase, public IHitReactionQuery
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		inputComponent_ = GetOwner()->GetComponent<PlayerInputComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		facingDirectionComponent_ = GetOwner()->GetComponent<FacingDirectionComponent>();
		lockOnComponent_ = GetOwner()->GetComponent<PlayerLockOnComponent>(); // ロックオン対象の選定/保持を担当する兄弟コンポーネント
		movementAnimationComponent_ = GetOwner()->GetComponent<PlayerMovementAnimationComponent>(); // Walk/Runの向き制御・アニメーション再生を担当する兄弟コンポーネント

		// コンボ各段のデータ(タイミング・踏み込み量等)をまとめて読み込む。
		comboAttacks_ = CreateDebugComboAttackTable();

		// Evade/Guardの基本データも同様にデバッグ用テーブルから読み込む。
		baseEvadeData_ = CreateDebugEvadeData();
		baseGuardData_ = CreateDebugGuardData();

		// 被弾時のパリィ/ガード/通常被弾の分岐と、それに伴うダメージ/
		// ノックバック/エフェクト処理はHitReactionComponentへ切り出した
		// (HurtBoxへのCollisionEnterEventの購読自体もHitReactionComponent側が
		// 持つため、ここでは購読処理を書かない)。ここでは「今パリィ猶予中か/
		// ガード中か/通常被弾でスタンへ入ってほしい」という問い合わせに
		// 答えられるよう、自分自身をIHitReactionQueryとして登録するだけでよい。
		if (HitReactionComponent* hitReaction = GetOwner()->GetComponent<HitReactionComponent>()) {
			hitReaction->SetQuerySource(this);
			hitReaction->SetWeaponCollider(weaponCollider_);
		}

		// 初期状態のセット。TransitionTo経由なのでEnterも呼ばれるが、
		TransitionTo(&stateNone_);
	}

	// --- 移動軸: 参照 --------------------------------------------------
	MovementState GetMovementState() const { return movementState_; }

	// --- 戦闘軸: 参照 (現在のStateに委譲) ------------------------------
	CombatState GetCombatState() const { return stateMachine_.Current()->GetDetailedState(); }
	float GetCombatElapsed() const { return stateMachine_.Current()->GetElapsed(); }

	bool IsAttacking() const {
		auto state = GetCombatState();
		return state == CombatState::AttackWindup || state == CombatState::AttackActive || state == CombatState::AttackRecovery;
	}

	bool IsEvading() const {
		auto state = GetCombatState();
		return state == CombatState::Evade || state == CombatState::EvadeRecovery;
	}

	// IHitReactionQuery実装。HitReactionComponentから、被弾時に
	// 「今ガード中か」を問い合わせるために呼ばれる。
	bool IsGuarding() const override { return GetCombatState() == CombatState::Guard; }

	bool IsStaggered() const {
		auto state = GetCombatState();
		return state == CombatState::StaggerSmall || state == CombatState::StaggerLarge;
	}

	// ジャスト判定もState側に委譲
	bool IsInJustEvadeWindow() const { return stateMachine_.Current()->IsInJustEvadeWindow(this); }

	// IHitReactionQuery実装。HitReactionComponentから、被弾時に
	// 「今パリィ猶予中か」を問い合わせるために呼ばれる。
	bool IsInParryWindow() const override { return stateMachine_.Current()->IsInParryWindow(this); }

	// --- 戦闘軸: 実行可否 (現在のStateに委譲) --------------------------
	bool CanStartAttack() const { return stateMachine_.Current()->CanStartAttack(this); }
	bool CanStartEvade() const { return stateMachine_.Current()->CanStartEvade(this); }
	bool CanStartGuard() const { return stateMachine_.Current()->CanStartGuard(this); }

	// --- データ取得 (Stateが判定に使うため) ----------------------------
	const AttackMoveData& GetCurrentAttackData() const { return currentAttack_; }
	const EvadeMoveData& GetCurrentEvadeData() const { return currentEvade_; }
	const GuardMoveData& GetCurrentGuardData() const { return currentGuard_; }

	// 回避方向を、Enter()時点の(=FacingDirectionComponentの追従が止まった直後の)
	// 現在の前方と比較し、前後左右のどれに該当するかを判定する。
	// StateEvade::Enter()が再生アニメーションを選ぶために呼ぶ(Evade専用)。
	EvadeDirection ClassifyEvadeDirection(const Math::Vector3& inputDirection) const {
		const Math::Vector3 forward = (transform_ != nullptr) ? transform_->GetForward() : Math::Vector3::Zero;
		return ::ClassifyEvadeDirection(forward, inputDirection);
	}

	// 現在のコンボ段数(0始まり、0=1段目)。演出・SE分岐等で参照したい場合用。
	int GetComboIndex() const { return comboIndex_; }

	// --- 状態遷移 (State内部から、あるいはControllerから呼ばれる) -------
	void ChangeStateToNone() { TransitionTo(&stateNone_); }

	bool TryStartAttack() {
		if (!CanStartAttack()) return false;

		currentAttack_ = comboAttacks_[comboIndex_];
		comboIndex_ = (comboIndex_ + 1) % kMaxComboHits; // 5段目の次は1段目へ折り返す

		stateMachine_.ForceTransitionTo(this, &stateAttack_);
		OnStateChanged(&stateAttack_);
		return true;
	}

	bool TryStartEvade(const EvadeMoveData& move) {
		if (!CanStartEvade()) return false;
		currentEvade_ = move;
		TransitionTo(&stateEvade_);
		return true;
	}

	bool TryStartGuard() {
		if (!CanStartGuard()) return false;
		currentGuard_ = baseGuardData_;
		TransitionTo(&stateGuard_);
		return true;
	}

	void ApplyStagger(bool isLarge, float duration) {
		stateStagger_.Setup(isLarge, duration);
		stateMachine_.ForceTransitionTo(this, &stateStagger_);
		OnStateChanged(&stateStagger_);
	}

	void EnterStagger(bool isLarge, float duration) override { ApplyStagger(isLarge, duration); }

	// --- ロックオン ------------------------------------------------------
	void TryLockOn() {
		if (lockOnComponent_ != nullptr) lockOnComponent_->TryLockOn();
	}
	void ClearLockOn() {
		if (lockOnComponent_ != nullptr) lockOnComponent_->ClearLockOn();
	}
	bool IsLockedOn() const {
		return lockOnComponent_ != nullptr && lockOnComponent_->IsLockedOn();
	}

	void FaceAttackTarget() {
		currentAttackTarget_ = {};

		if (lockOnComponent_ == nullptr || transform_ == nullptr) return;

		GameObject* target = lockOnComponent_->IsLockedOn()
			? lockOnComponent_->GetLockedTarget()
			: lockOnComponent_->FindNearestToScreenCenter();

		currentAttackTarget_ = Handle<GameObject>(target);

		FaceTowards(target);
	}

	// --- Stateからの移動リクエスト --------------------------------------
	void RequestStepMove(const Math::Vector3& direction, float distance, float duration);

	void RequestStepMoveTowardsTarget(const Math::Vector3& fallbackDirection, float stepDistance,
		float engageDistance, float duration);

	void CancelStepMove();

	// --- 武器の攻撃判定 --------------------------------------------------
	void SetWeapon(Handle<ColliderComponent> weaponCollider, Handle<AttackSourceComponent> weaponAttackSource,
		Handle<TrailPolygonComponent> weaponTrail = {}) {
		weaponCollider_ = weaponCollider;
		weaponAttackSource_ = weaponAttackSource;
		weaponTrail_ = weaponTrail;
	}

	void SetWeaponHitBoxEnabled(bool enabled) {
		if (ColliderComponent* collider = weaponCollider_.Resolve()) {
			collider->SetShapeEnabled("HitBox", enabled);
		}

		if (enabled) {
			if (AttackSourceComponent* source = weaponAttackSource_.Resolve()) {
				source->alreadyHit.clear();
			}
		}
	}

	void SetWeaponTrailEmitting(bool emitting) {
		if (TrailPolygonComponent* trail = weaponTrail_.Resolve()) {
			if (emitting) {
				trail->StartEmit();
			}
			else {
				trail->StopEmit();
			}
		}
	}

	// --- アニメーション再生 -----------------------------------------------
	// State側が具体的なModelAnimatorComponentを直接知らずに再生できるようにする
	// 薄いラッパー。Attack/Evade/Guardのようにルートモーションやフェーズごとの
	// ブレンド時間が絡む複雑な再生はこちらに残している(Walk/Runの向き制御・
	// Start/Loop/End/ターンはPlayerMovementAnimationComponent側に切り出し済み)。
	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f,
		bool useRootMotion = false, float blendDurationSeconds = kDefaultAnimationBlendDuration) {
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->SetRootMotionBoneName(useRootMotion ? kRootMotionBoneName : "");
			modelAnimatorComponent_->SetBlendDuration(blendDurationSeconds);
			modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
		}
	}

	// 現在のMovementStateに応じたアニメーションを再生し直す。
	// StateNone::Enter()(=攻撃/回避/ガード/怯みが終わった直後)から呼ばれる。
	// 実際の再生処理・向き制御はPlayerMovementAnimationComponent::Refresh()
	// へ委譲する。
	void RefreshMovementAnimation() {
		if (inputComponent_ == nullptr) return;
		movementState_ = inputComponent_->GetDesiredMovementState();
		ApplyMovementState(movementState_);

		if (movementAnimationComponent_ != nullptr) {
			movementAnimationComponent_->Refresh(movementState_, inputComponent_->GetMoveDirection(), IsLockedOn());
		}
	}

	// --- ライフサイクル --------------------------------------------------
	void Update(float deltaTime) override
	{
		if (inputComponent_ != nullptr) {
			HandleMovementInput(*inputComponent_, deltaTime);
			HandleActionInput(*inputComponent_);
		}

		// ロック中は、移動(None状態)中であっても向きをロック対象方向へ固定する。
		// ただしRun中は要件によりロック有無に関わらず入力方向へ正対するため、
		// UpdateLockOnFacing側でRunをスキップする。
		UpdateLockOnFacing();

		UpdateMovementState(deltaTime);

		// 戦闘状態の更新は共通StateMachineに丸投げ
		stateMachine_.Update(this, deltaTime);

		// ルートモーションの反映は、同じGameObjectに付くRootMotionApplier
		// Component側が自分のUpdate()で毎フレーム自律的に行う。PlayerFactory
		// 側でModelAnimatorComponent/TransformComponentと合わせてこの
		// コンポーネント、およびPlayerMovementAnimationComponentを付けておくこと。
	}

private:
	void TransitionTo(IPlayerState* nextState) {
		if (stateMachine_.TransitionTo(this, nextState)) {
			OnStateChanged(nextState);
		}
	}

	void OnStateChanged(IPlayerState* nextState) {
		if (movementComponent_) {
			movementComponent_->SetEnabled(nextState == &stateNone_);
		}
		if (facingDirectionComponent_) {
			facingDirectionComponent_->SetUpdateEnabled(nextState == &stateNone_);
		}
		if (nextState != &stateAttack_) {
			comboIndex_ = 0;
		}
	}

	void HandleMovementInput(const PlayerInputComponent& input, float deltaTime);
	void HandleActionInput(PlayerInputComponent& input);
	void ApplyMovementState(MovementState state);
	void UpdateMovementState(float deltaTime);

	void FaceTowards(GameObject* target) {
		if (target == nullptr || transform_ == nullptr) return;

		TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return;

		Math::Vector3 dir = targetTransform->GetPosition() - transform_->GetPosition();
		dir.y = 0.0f;
		if (dir.LengthSquared() <= kDirectionEpsilon) return;
		dir.Normalize();
		dir = -dir;

		const float yaw = std::atan2(dir.x, dir.z);
		transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
	}

	// ロック中、移動(CombatState::None)中でも向きをロック対象へ固定する。
	// 【変更】非ロック中のWalk/Runの向き制御(入力方向への追従、ターン)は
	// PlayerMovementAnimationComponent側(FaceDirection/BeginTurnOrStart)へ
	// 完全に移管したため、FacingDirectionComponentの自動追従はここでは
	// 常に無効化しておく。有効なままにしていると、PlayerMovementAnimation
	// Componentが向きを制御する前に(あるいは同じフレーム中に)
	// FacingDirectionComponent自身が移動方向へtransform_を回転させてしまい、
	// ClassifyTurnDirectionが「向きを変える前の状態」を参照できず常に
	// TurnDirection::Noneと判定される不具合の原因になっていた。
	//
	// 【要件】走行(Run)はロック有無に関わらず常に入力方向へ正対するため、
	// ここでは何もしない(PlayerMovementAnimationComponent::Tick側が処理)。
	void UpdateLockOnFacing() {
		if (facingDirectionComponent_ == nullptr) return;
		if (GetCombatState() != CombatState::None) return;

		facingDirectionComponent_->SetUpdateEnabled(false);

		if (movementState_ == MovementState::Run) return;

		const bool lockedOn = lockOnComponent_ != nullptr && lockOnComponent_->IsLockedOn();
		if (lockedOn) {
			FaceTowards(lockOnComponent_->GetLockedTarget());
		}
	}

	// 兄弟コンポーネント
	PlayerInputComponent* inputComponent_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;
	FacingDirectionComponent* facingDirectionComponent_ = nullptr;
	PlayerLockOnComponent* lockOnComponent_ = nullptr;
	PlayerMovementAnimationComponent* movementAnimationComponent_ = nullptr;

	// 武器(別GameObject、ソケット経由でアタッチ)への弱参照。
	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;
	Handle<TrailPolygonComponent> weaponTrail_;

	Handle<GameObject> currentAttackTarget_;

	// --- 移動データ ---
	MovementState movementState_ = MovementState::Stand;
	float walkSpeed_ = 2.0f;
	float runSpeed_ = 5.0f;

	// PlayAnimation()でblendDurationSecondsを省略した場合に使う既定値。
	static constexpr float kDefaultAnimationBlendDuration = 0.15f;

	// PlayAnimation(useRootMotion=true)の際に使うボーン名。
	static constexpr const char* kRootMotionBoneName = "root";

	// --- 戦闘データ ---
	AttackMoveData currentAttack_;
	EvadeMoveData currentEvade_;
	GuardMoveData currentGuard_;

	EvadeMoveData baseEvadeData_;
	GuardMoveData baseGuardData_;

	int comboIndex_ = 0;
	ComboAttackTable comboAttacks_;

	// --- Stateインスタンス (メモリ断片化を防ぐため実体をメンバで持つ) ---
	StateNone    stateNone_;
	StateAttack  stateAttack_;
	StateEvade   stateEvade_;
	StateGuard   stateGuard_;
	StateStagger stateStagger_;

	StateMachine<PlayerStatusController, IPlayerState> stateMachine_;
};