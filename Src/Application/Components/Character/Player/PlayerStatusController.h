// PlayerStatusController.h
#pragma once
#include <array>
#include <string>
#include "PlayerCombatTypes.h"
#include "PlayerInputComponent.h"
#include "../../Movement/MovementComponent.h" // 既存の依存として
#include "PlayerState.h"
#include "../StateMachine/StateMachine.h"
#include "../../Animation/ModelAnimatorComponent.h"
#include "../../Collision/ColliderComponent.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../../../Core/Handle.h"

class PlayerStatusController : public ComponentBase
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		inputComponent_ = GetOwner()->GetComponent<PlayerInputComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();

		// コンボ各段のアニメーション名を仮で割り当てる
		// (本番の技データテーブルが用意できたらここは不要になる)。
		for (int i = 0; i < kMaxComboHits; ++i) {
			comboAttacks_[i].animationName = "Attack" + std::to_string(i + 1);
		}

		// 初期状態のセット。TransitionTo経由なのでEnterも呼ばれるが、
		// StateNone::Enter()がRefreshMovementAnimation()を呼ぶため、
		// ここで最初のIdleアニメーションも合わせて再生される。
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

	bool IsGuarding() const { return GetCombatState() == CombatState::Guard; }

	bool IsStaggered() const {
		auto state = GetCombatState();
		return state == CombatState::StaggerSmall || state == CombatState::StaggerLarge;
	}

	// ジャスト判定もState側に委譲
	bool IsInJustEvadeWindow() const { return stateMachine_.Current()->IsInJustEvadeWindow(this); }
	bool IsInParryWindow() const { return stateMachine_.Current()->IsInParryWindow(this); }

	// --- 戦闘軸: 実行可否 (現在のStateに委譲) --------------------------
	// Guardもここに揃える(以前はHandleActionInput内でCombatState::Noneを
	// 直接比較していたが、Attack/Evadeと同じくState側のポリモーフィズムに乗せる)。
	bool CanStartAttack() const { return stateMachine_.Current()->CanStartAttack(this); }
	bool CanStartEvade() const { return stateMachine_.Current()->CanStartEvade(this); }
	bool CanStartGuard() const { return stateMachine_.Current()->CanStartGuard(this); }

	// --- データ取得 (Stateが判定に使うため) ----------------------------
	const AttackMoveData& GetCurrentAttackData() const { return currentAttack_; }
	const EvadeMoveData& GetCurrentEvadeData() const { return currentEvade_; }
	const GuardMoveData& GetCurrentGuardData() const { return currentGuard_; }

	// 現在のコンボ段数(0始まり、0=1段目)。演出・SE分岐等で参照したい場合用。
	int GetComboIndex() const { return comboIndex_; }

	// --- 状態遷移 (State内部から、あるいはControllerから呼ばれる) -------
	void ChangeStateToNone() { TransitionTo(&stateNone_); }

	// コンボ何段目を出すかは内部のcomboIndex_で判断するため、
	// 呼び出し側は技データを組み立てず、ただ「攻撃したい」とだけ伝える。
	bool TryStartAttack() {
		if (!CanStartAttack()) return false;

		currentAttack_ = comboAttacks_[comboIndex_];
		comboIndex_ = (comboIndex_ + 1) % kMaxComboHits; // 5段目の次は1段目へ折り返す

		TransitionTo(&stateAttack_);
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
		currentGuard_ = GuardMoveData{};
		TransitionTo(&stateGuard_);
		return true;
	}

	void ApplyStagger(bool isLarge, float duration) {
		stateStagger_.Setup(isLarge, duration);
		// Staggerは同じStateインスタンスへの再突入がありうる(連続ヒット等)。
		// 通常のTransitionToは「同じインスタンスなら何もしない」ため、
		// ForceTransitionToで必ずExit→Enterし直す。
		stateMachine_.ForceTransitionTo(this, &stateStagger_);
		OnStateChanged(&stateStagger_);
	}

	// --- Stateからの移動リクエスト --------------------------------------
	// 現在位置から指定方向へdistanceだけduration秒かけて移動する。
	// directionがほぼゼロベクトルの場合はモデルの向いている方向へ
	// フォールバックする。具体的なコンポーネント実装(TweenMoveComponent等)は
	// ここに閉じ込め、StateはTransformComponent/TweenMoveComponentを
	// 直接知らなくて済むようにする。
	void RequestStepMove(const Math::Vector3& direction, float distance, float duration);

	// 進行中のステップ移動があれば止める(フェーズ遷移や状態の中断時に使う)。
	void CancelStepMove();

	// --- 武器の攻撃判定 --------------------------------------------------
	// PlayerFactory::CreatePlayer側で、生成した武器のColliderComponent/
	// AttackSourceComponentをここに登録してもらう想定
	// (武器はソケット経由でアタッチされる別GameObjectのため、Handle経由の
	//  弱参照で保持する。武器が破棄された場合はResolve()がnullptrを返す)。
	void SetWeapon(Handle<ColliderComponent> weaponCollider, Handle<AttackSourceComponent> weaponAttackSource) {
		weaponCollider_ = weaponCollider;
		weaponAttackSource_ = weaponAttackSource;
	}

	// StateAttack::Updateから、AttackActiveフェーズの開始/終了に合わせて
	// 呼ばれる。攻撃判定(HitBox形状)のenabled切り替えと、多段ヒット防止用の
	// 記録(AttackSourceComponent::alreadyHit)のクリアをここに集約する。
	//
	// 常時enabled=trueのままだと、HurtBoxと重なり続けている間
	// CollisionEnterEventが繰り返し発火し、そのたびにノックバックが
	// 積み増される不具合があった(詳細は経緯コメント不要、実際に発生した
	// 不具合)。攻撃が実際に発生している一瞬だけ判定させることで解決する。
	void SetWeaponHitBoxEnabled(bool enabled) {
		if (ColliderComponent* collider = weaponCollider_.Resolve()) {
			collider->SetShapeEnabled("HitBox", enabled);
		}

		if (enabled) {
			// 新しい攻撃の開始として、前回までのヒット記録をクリアする。
			// (無効化する側=Recovery移行時にクリアしないのは、Recovery中に
			//  誰かがalreadyHitを覗きに来る可能性を考慮し、次の攻撃が
			//  始まる直前まで記録を残しておくため)
			if (AttackSourceComponent* source = weaponAttackSource_.Resolve()) {
				source->alreadyHit.clear();
			}
		}
	}

	// --- アニメーション再生 -----------------------------------------------
	// State側が具体的なModelAnimatorComponentを直接知らずに再生できるようにする
	// 薄いラッパー(RequestStepMoveと同じ考え方)。
	void PlayAnimation(const std::string& name, bool loop = false) {
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->Play(name, loop);
		}
	}

	// 現在のMovementStateに応じたアニメーションを再生し直す。
	// StateNone::Enter()(=攻撃/回避/ガード/怯みが終わった直後)から呼ばれ、
	// 「行動中に入力状態が変わっていても、Noneに戻った瞬間に必ず現在の
	// 入力を反映したアニメーションに同期させる」ために使う
	// (HandleMovementInputの「変化があった時だけPlay」する仕組みだけでは、
	//  行動中ずっと同じ入力のままだった場合にPlay()が呼ばれず、
	//  行動アニメーションの最終ポーズで固まったままになってしまうため)。
	void RefreshMovementAnimation() {
		if (inputComponent_ == nullptr) return;
		movementState_ = inputComponent_->GetDesiredMovementState();
		ApplyMovementState(movementState_);
		PlayMovementAnimation(movementState_);
	}

	// --- ライフサイクル --------------------------------------------------
	void Update(float deltaTime) override
	{
		if (inputComponent_ != nullptr) {
			HandleMovementInput(*inputComponent_);
			HandleActionInput(*inputComponent_);
		}

		UpdateMovementState(deltaTime);

		// 戦闘状態の更新は共通StateMachineに丸投げ
		stateMachine_.Update(this, deltaTime);
	}

private:
	void TransitionTo(IPlayerState* nextState) {
		if (stateMachine_.TransitionTo(this, nextState)) {
			OnStateChanged(nextState);
		}
	}

	// 移動の許可/禁止はここに集約する。None以外(攻撃/回避/ガード/怯み)の
	// 間はMovementComponentごと無効化し、各Stateが個別に
	// enable/disableを気にしなくて済むようにする。
	//
	// あわせて、コンボの連鎖もここで判定する。Attack以外へ遷移した場合
	// (Recovery中のコンボキャンセルでstateAttack_へ再突入するケースを
	//  除く)はコンボが途切れたとみなし、comboIndex_を1段目へ戻す。
	void OnStateChanged(IPlayerState* nextState) {
		if (movementComponent_) {
			movementComponent_->SetEnabled(nextState == &stateNone_);
		}
		if (nextState != &stateAttack_) {
			comboIndex_ = 0;
		}
	}

	void HandleMovementInput(const PlayerInputComponent& input);
	void HandleActionInput(PlayerInputComponent& input);
	void ApplyMovementState(MovementState state);
	void UpdateMovementState(float deltaTime);

	// Stand/Walk/Runそれぞれのアニメーションをループ再生する。
	// アイドルを基本状態として扱うため、Standでは明示的にkIdleAnimationを再生する。
	void PlayMovementAnimation(MovementState state) {
		switch (state) {
		case MovementState::Stand: PlayAnimation(kIdleAnimation, true); break;
		case MovementState::Walk:  PlayAnimation(kWalkAnimation, true); break;
		case MovementState::Run:   PlayAnimation(kRunAnimation, true); break;
		}
	}

	// 兄弟コンポーネント
	PlayerInputComponent* inputComponent_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;

	// 武器(別GameObject、ソケット経由でアタッチ)への弱参照。
	// SetWeapon()経由でPlayerFactory側からセットされる想定。
	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;

	// --- 移動データ ---
	MovementState movementState_ = MovementState::Stand;
	float walkSpeed_ = 2.0f;
	float runSpeed_ = 5.0f;

	// Stand/Walk/Runのアニメーション名(仮)。
	static constexpr const char* kIdleAnimation = "Idle";
	static constexpr const char* kWalkAnimation = "Walk";
	static constexpr const char* kRunAnimation = "Run";

	// --- 戦闘データ ---
	AttackMoveData currentAttack_;
	EvadeMoveData currentEvade_;
	GuardMoveData currentGuard_;

	// コンボ攻撃: 何段目か(0始まり)と、各段の技データテーブル。
	// 各要素の具体的な数値(モーション時間・踏み込み距離等)は別途詰める
	// (現状は全段ともAttackMoveDataのデフォルト値+仮のanimationNameのまま)。
	int comboIndex_ = 0;
	std::array<AttackMoveData, kMaxComboHits> comboAttacks_;

	// --- Stateインスタンス (メモリ断片化を防ぐため実体をメンバで持つ) ---
	StateNone    stateNone_;
	StateAttack  stateAttack_;
	StateEvade   stateEvade_;
	StateGuard   stateGuard_;
	StateStagger stateStagger_;

	// 共通StateMachine基盤(遷移ロジック・現在Stateの保持)
	StateMachine<PlayerStatusController, IPlayerState> stateMachine_;
};