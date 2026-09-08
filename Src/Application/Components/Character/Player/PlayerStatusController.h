#pragma once

#include <array>
#include <cmath>
#include <string>
#include <vector>
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
#include "../../Combat/WeaponSetComponent.h"
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
		weaponSet_ = GetOwner()->GetComponent<WeaponSetComponent>(); // 武器/攻撃部位の制御を担当する兄弟コンポーネント(Enemyとも共有する汎用実装)

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
			// メイン武器のColliderをHitReactionComponentへ渡す。Handleは
			// WeaponComponent側に用意させず、受け渡し時にこちらで組み立てる
			// (WeaponComponentはHandleを一切扱わない設計。WeaponComponent.h参照)。
			// 装備(WeaponSetComponent::RegisterWeapon)がこのStart()より後に
			// 行われる構成の場合はまだ未登録でnullptrになりうる点は
			// 旧実装(weaponCollider_直持ち)と同じ制約。
			if (weaponSet_ != nullptr) {
				if (WeaponComponent* mainWeapon = weaponSet_->GetWeapon(kMainWeaponSlot)) {
					if (ColliderComponent* mainWeaponCollider = mainWeapon->GetCollider()) {
						hitReaction->SetWeaponCollider(Handle<ColliderComponent>(mainWeaponCollider));
					}
				}
			}
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

	// ガードキーを離した際に即座に解除してよいか(パリィ成功演出中はfalse)。
	bool CanReleaseGuard() const { return stateMachine_.Current()->CanReleaseGuard(this); }

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
		comboIndex_ = (comboIndex_ + 1) % kMaxComboHits; // 最終段の次は1段目へ折り返す(kMaxComboHits段)

		ForceTransitionTo(&stateAttack_);
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
		ForceTransitionTo(&stateStagger_);
	}

	void EnterStagger(bool isLarge, float duration) override { ApplyStagger(isLarge, duration); }

	// IHitReactionQuery実装。HitReactionComponentから、自分自身がパリィに
	// 成功した際に呼ばれる。パリィはGuard中にしか成立し得ないため、
	// Guard中でなければ何もしない(念のためのガード)。
	void NotifyParrySuccess() override {
		if (GetCombatState() == CombatState::Guard) {
			stateGuard_.NotifyParrySuccess(this);
		}
	}

	// IHitReactionQuery実装。HitReactionComponentから、自分自身が通常
	// ブロックで被弾した際に呼ばれる。
	void NotifyGuardHit() override {
		if (GetCombatState() == CombatState::Guard) {
			stateGuard_.NotifyGuardHit(this);
		}
	}

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
	// 実体の制御はWeaponSetComponent/WeaponComponent(Enemyとも共有する
	// 汎用実装)へ委譲する薄いラッパー。Playerは常に単一武器という前提だけを
	// ここで吸収し、呼び出し側(HandleActionInput/State側)は今まで通り
	// PlayerStatusController経由で操作できるようにする。
	void SetWeapon(Handle<WeaponComponent> weapon) {
		// 【重要】PlayerFactory等は、まだStart()が呼ばれていない構築中の
		// タイミング(AddComponent<PlayerStatusController>()した直後)で
		// このSetWeapon()を呼ぶ。weaponSet_はStart()内で初めて解決される
		// キャッシュのため、ここで参照するとまだnullptrで登録が握りつぶされる。
		// WeaponSetComponent自体は既にAddComponent済み(AttachPhysics相当)の
		// はずなので、ここだけは都度GetComponent()で解決する。
		if (WeaponSetComponent* weaponSet = GetOwner()->GetComponent<WeaponSetComponent>()) {
			weaponSet->RegisterWeapon(kMainWeaponSlot, weapon);
		}
	}

	// slotsはAttackMoveData::weaponSlots(技データ側)から渡される。
	// Playerの技は基本{"Main"}固定だが、将来二刀流等でスロットが増えても
	// このシグネチャのまま対応できる。
	void SetWeaponHitBoxEnabled(const std::vector<std::string>& slots, bool enabled) {
		if (weaponSet_ != nullptr) {
			weaponSet_->SetHitBoxEnabled(slots, enabled);
		}
	}

	void SetWeaponTrailEmitting(const std::vector<std::string>& slots, bool emitting) {
		if (weaponSet_ != nullptr) {
			weaponSet_->SetTrailEmitting(slots, emitting);
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

		UpdateLockOnFacing();
		UpdateMovementState(deltaTime);

		// Recovery自然終了後のコンボ継続受付ウィンドウを消化する。
		// (Attack中やウィンドウ非該当の遷移ではOnStateChanged側で既に
		//  0にしているため、ここでは単純にカウントダウンするだけでよい)
		if (comboWindowRemaining_ > 0.0f) {
			comboWindowRemaining_ -= deltaTime;
			if (comboWindowRemaining_ <= 0.0f) {
				comboWindowRemaining_ = 0.0f;
				comboIndex_ = 0;
			}
		}

		// 戦闘状態の更新は共通StateMachineに丸投げ
		stateMachine_.Update(this, deltaTime);
	}

private:
	// 通常の(失敗しうる)遷移。prevStateをOnStateChanged側で参照できるよう、
	// 遷移前にCurrent()を取得してから渡す。
	void TransitionTo(IPlayerState* nextState) {
		IPlayerState* prevState = stateMachine_.Current();
		if (stateMachine_.TransitionTo(this, nextState)) {
			OnStateChanged(prevState, nextState);
		}
	}

	// 強制(必ず成功する)遷移。TryStartAttack/ApplyStaggerのように
	// CanStartXxx()を独自に判定済みで、StateMachine側の可否判定を
	// バイパスしたい場合に使う。
	void ForceTransitionTo(IPlayerState* nextState) {
		IPlayerState* prevState = stateMachine_.Current();
		stateMachine_.ForceTransitionTo(this, nextState);
		OnStateChanged(prevState, nextState);
	}

	void OnStateChanged(IPlayerState* prevState, IPlayerState* nextState) {
		if (movementComponent_) {
			movementComponent_->SetEnabled(nextState == &stateNone_);
		}
		if (facingDirectionComponent_) {
			facingDirectionComponent_->SetUpdateEnabled(nextState == &stateNone_);
		}

		if (nextState == &stateAttack_) {
			// 新しい攻撃を開始した時点で、Recovery後のコンボ継続受付
			// ウィンドウはもう不要(comboIndex_自体はTryStartAttack側で
			// 既に進めている)。
			comboWindowRemaining_ = 0.0f;
			return;
		}

		if (prevState == &stateAttack_ && nextState == &stateNone_) {
			// AttackのRecoveryが(中断されずに)自然終了してNoneへ戻った
			// 場合のみ、currentAttack_.comboWindowAfterRecovery秒だけ
			// comboIndex_を維持し、次の攻撃入力をコンボ継続として扱う。
			comboWindowRemaining_ = currentAttack_.comboWindowAfterRecovery;
			if (comboWindowRemaining_ <= 0.0f) {
				comboIndex_ = 0;
			}
			return;
		}

		// Evade/Guard/Staggerへの割り込みなど、それ以外の遷移では
		// コンボ継続を認めず即座に打ち切る。
		comboIndex_ = 0;
		comboWindowRemaining_ = 0.0f;
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

	// ロック中、移動中でも向きをロック対象へ固定する。
	void UpdateLockOnFacing() {
		if (facingDirectionComponent_ == nullptr) return;
		if (GetCombatState() != CombatState::None) return;

		facingDirectionComponent_->SetUpdateEnabled(true);

		// 走行中は方向ロック解除
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
	WeaponSetComponent* weaponSet_ = nullptr; // 武器/攻撃部位の集合(Enemyとも共有する汎用コンポーネント)

	Handle<GameObject> currentAttackTarget_;

	// --- 移動データ ---
	MovementState movementState_ = MovementState::Stand;
	float walkSpeed_ = 2.0f;
	float runSpeed_ = 5.0f;

	// PlayAnimation()でblendDurationSecondsを省略した場合に使う既定値。
	static constexpr float kDefaultAnimationBlendDuration = 0.15f;

	// PlayAnimation(useRootMotion=true)の際に使うボーン名。
	static constexpr const char* kRootMotionBoneName = "root";

	// Playerは常に単一武器のため、WeaponSetComponent上のスロット名を固定する。
	// (SetWeapon()での登録・HitReactionComponentへの受け渡しの両方で使用)
	static constexpr const char* kMainWeaponSlot = "Main";

	// --- 戦闘データ ---
	AttackMoveData currentAttack_;
	EvadeMoveData currentEvade_;
	GuardMoveData currentGuard_;

	EvadeMoveData baseEvadeData_;
	GuardMoveData baseGuardData_;

	int comboIndex_ = 0;

	// Recovery自然終了後、この秒数が残っている間はNone状態でも
	// comboIndex_を維持する(OnStateChangedで設定、Update()で消化)。
	float comboWindowRemaining_ = 0.0f;

	ComboAttackTable comboAttacks_;

	// --- Stateインスタンス (メモリ断片化を防ぐため実体をメンバで持つ) ---
	StateNone    stateNone_;
	StateAttack  stateAttack_;
	StateEvade   stateEvade_;
	StateGuard   stateGuard_;
	StateStagger stateStagger_;

	StateMachine<PlayerStatusController, IPlayerState> stateMachine_;
};