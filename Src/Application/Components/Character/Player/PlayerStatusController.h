#pragma once
#include <array>
#include <string>
#include "PlayerCombatTypes.h"
#include "PlayerCombatDataTable.h"
#include "PlayerInputComponent.h"
#include "../../Movement/MovementComponent.h" // 既存の依存として
#include "../../Movement/VelocityComponent.h"
#include "../../Transform/TransformComponent.h"
#include "PlayerState.h"
#include "../StateMachine/StateMachine.h"
#include "../../Animation/ModelAnimatorComponent.h"
#include "../../Collision/ColliderComponent.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../Data/PostureComponent.h"
#include "../Data/HealthComponent.h"
#include "../Enemy/EnemyStatusController.h"
#include "../../../Core/Handle.h"
#include "../../../Systems/Collision/CollisionSystem.h"

class PlayerStatusController : public ComponentBase
{
public:
	explicit PlayerStatusController(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		inputComponent_ = GetOwner()->GetComponent<PlayerInputComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
		postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
		velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// コンボ各段のデータ(タイミング・踏み込み量等)をまとめて読み込む。
		// 現状はデバッグ用の直書きテーブル(CreateDebugComboAttackTable())
		// から取得しているが、将来的にはJSON等の外部データから読み込んだ
		// テーブルをそのまま代入できるようにするための下準備
		// (実際のJSON読み込み処理自体はまだ未実装)。
		comboAttacks_ = CreateDebugComboAttackTable();

		// Evade/Guardの基本データも同様にデバッグ用テーブルから読み込む。
		// Evadeは方向(evadeDirection)だけ入力時に上書きされ(HandleActionInput
		// 参照)、それ以外のタイミング系はこの基本データをそのまま使う。
		baseEvadeData_ = CreateDebugEvadeData();
		baseGuardData_ = CreateDebugGuardData();

		// HurtBoxへのCollisionEnterEventは、シーン共有バスではなく
		// このGameObject自身のローカルバスにだけ届く(CollisionSystem/
		// EnemyStatusController::Start()の同種のコメント参照)。
		EventBus& localBus = GetOwner()->GetLocalEventBus();
		const SubscriptionId subscriptionId = localBus.Subscribe<CollisionSystem::CollisionEnterEvent>(
			[this](const CollisionSystem::CollisionEnterEvent& e) { OnCollisionEnter(e); });
		subscriber_ = ScopedSubscriber(&localBus, subscriptionId);

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

		// コンボの2段目以降は同一StateAttackインスタンスへの再突入になる。
		// 通常のTransitionTo()は「同一インスタンスなら何もしない」ため、
		// phase_/elapsed_がAttackRecoveryのまま引き継がれてしまい、
		// 2段目のWindupが始まらずそのままRecovery終了→Noneに戻ってしまう
		// 不具合があった。ForceTransitionToで必ずExit→Enterし直す
		// (ApplyStagger()と同じ考え方)。
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
	//
	// targetDurationSecondsを渡すと、アニメーションクリップの実際の長さに
	// 関わらず、その秒数でちょうど再生し終わるよう速度を自動調整する
	// (ModelAnimatorComponent::Play()参照)。攻撃/回避のように、
	// ゲームプレイ側の秒数(AttackMoveData等)を基準にしたい場合に使う。
	//
	// blendDurationSecondsは、この呼び出しで切り替わる際のクロスフェード
	// 時間。呼ぶたびに必ずModelAnimatorComponent::SetBlendDuration()で
	// 明示的に設定し直すことで、「前回どこかで設定した値が意図せず
	// 引き継がれる」ことを防いでいる(省略時はkDefaultAnimationBlendDuration、
	// コンボ攻撃のようにAttackMoveData側で個別の値を持つ場合はそちらを渡す。
	// 詳細はStateAttack::Enter()参照)。
	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f,
		float blendDurationSeconds = kDefaultAnimationBlendDuration) {
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->SetBlendDuration(blendDurationSeconds);
			modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
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
	// 被弾によるStagger強制遷移もAttack以外への遷移として扱われるため、
	// 「被弾したらコンボは途切れる」仕様になる(意図した挙動)。
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

	// --- 被弾時の分岐(パリィ/ガード/通常被弾) ---------------------------
	// HurtBoxへのCollisionEnterEvent受信時に呼ばれる。
	// 「攻撃を受けた瞬間、今がジャスト猶予中かガード中か」をStateMachine
	// (IsInParryWindow/IsGuarding)に問い合わせて分岐する。
	//
	// HP(HealthComponent)は未実装のため、通常被弾時のダメージ処理と
	// ガード時のチップダメージ処理はTODOのまま。
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e) {
		if (e.selfShapeName != "HurtBox") return;

		AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
		if (attack == nullptr) return;

		// 多段ヒット防止(EnemyStatusController::OnCollisionEnterと同じ考え方)。
		if (attack->alreadyHit.count(GetOwner()) > 0) return;
		attack->alreadyHit.insert(GetOwner());

		if (IsInParryWindow()) {
			// 完全パリィ: 自分は体幹・HPともにノーダメージ。
			// 攻撃者本体(ownerCharacter経由)の体幹を大きく削る。
			// ownerCharacterが未設定/破棄済み(Resolve()==nullptr)の場合は
			// 何もしない(現状Enemy側の攻撃は仮実装のため、この経路が
			// 実際に通ることはまだ無い想定)。
			if (GameObject* attacker = attack->ownerCharacter.Resolve()) {
				if (PostureComponent* attackerPosture = attacker->GetComponent<PostureComponent>()) {
					attackerPosture->AddPostureDamage(attack->parryPostureDamage);
				}

				// 現状は敵本体がAttackSourceComponentを直接持つ仮実装のため、
				// GameObject経由でEnemyStatusControllerを直接呼び出している。
				// この「PlayerがEnemyの具体型を直接知る」という結合は、
				// 将来Player同様の武器オブジェクトをEnemyも持つようになり、
				// 汎用的な「パリィされた時の反応」インターフェースへ整理する
				// 際に見直すこと(現時点では意図的な仮実装)。
				if (EnemyStatusController* enemyStatus = attacker->GetComponent<EnemyStatusController>()) {
					enemyStatus->ChangeStateToParryStun();
				}
			}
			// TODO: 弾き返しの演出(SE/VFX)は別途実装。
		}
		else if (IsGuarding()) {
			// 通常ブロック: 自分の体幹を削り、HPにも軽減済みのチップ
			// ダメージを適用する。
			if (postureComponent_ != nullptr) {
				postureComponent_->AddPostureDamage(attack->postureDamage);
				if (postureComponent_->IsBroken()) {
					// TODO: 崩し状態(専用State)への遷移は別途実装。
				}
			}
			if (healthComponent_ != nullptr) {
				healthComponent_->TakeDamage(attack->damage * attack->chipDamageRatio);
			}
		}
		else {
			// 通常被弾: ダメージ+体幹ダメージ+ノックバックを付与し、
			// 体幹が壊れたかどうかで小スタン/大スタンに分岐する。
			//
			// ノックバック方向は幾何学的接触法線(hitNormal)ではなく、
			// 攻撃者→自分への水平方向を使う(Enemy側で発生した「Box形状の
			// 深い重なりでhitNormalが攻撃と無関係な軸を返し、地面を
			// すり抜ける」不具合と同じ原因を最初から避けるため。詳細は
			// EnemyStatusController::OnCollisionEnterの同種のコメント参照)。
			if (healthComponent_ != nullptr) {
				healthComponent_->TakeDamage(attack->damage);
			}

			if (velocityComponent_ != nullptr && transform_ != nullptr) {
				Math::Vector3 dir = transform_->GetForward();
				if (GameObject* attacker = attack->ownerCharacter.Resolve()) {
					if (TransformComponent* attackerTransform = attacker->GetComponent<TransformComponent>()) {
						dir = transform_->GetPosition() - attackerTransform->GetPosition();
						dir.y = 0.0f;
						if (dir.LengthSquared() < 1e-6f) {
							dir = transform_->GetForward();
						}
						else {
							dir.Normalize();
						}
					}
				}
				velocityComponent_->AddImpulse(dir * attack->knockbackPower);
			}

			// 通常被弾でも体幹にダメージを蓄積する(ガード時とは異なり全ダメージ分)。
			// 体幹が壊れた(IsBroken())場合は大スタン(のけぞり)へ、
			// そうでなければ従来通り小スタン(よろけ、attack->hitStunSeconds)へ。
			bool postureBroken = false;
			if (postureComponent_ != nullptr) {
				postureComponent_->AddPostureDamage(attack->postureDamage);
				postureBroken = postureComponent_->IsBroken();
				if (postureBroken) {
					// 大スタンへ移行するため、次の蓄積に備えて体幹ゲージを空にする。
					// TODO: PostureComponent側の実際のリセットAPI名に合わせて修正すること
					// (現状Reset()という名称を仮定している)。
					postureComponent_->Reset();
				}
			}

			ApplyStagger(/*isLarge=*/postureBroken, postureBroken ? kLargeStaggerDuration : attack->hitStunSeconds);
		}
	}

	// 兄弟コンポーネント
	PlayerInputComponent* inputComponent_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;

	// 武器(別GameObject、ソケット経由でアタッチ)への弱参照。
	// SetWeapon()経由でPlayerFactory側からセットされる想定。
	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;

	ScopedSubscriber subscriber_;

	// --- 移動データ ---
	MovementState movementState_ = MovementState::Stand;
	float walkSpeed_ = 2.0f;
	float runSpeed_ = 5.0f;

	// Stand/Walk/Runのアニメーション名(仮)。
	static constexpr const char* kIdleAnimation = "Idle";
	static constexpr const char* kWalkAnimation = "Walk";
	static constexpr const char* kRunAnimation = "Walk"; //"Run";

	// 大スタン(のけぞり)の硬直時間(仮)。小スタンはattack->hitStunSecondsを
	// そのまま使うが、大スタンは攻撃側のデータに依存させず一律の値にしている。
	static constexpr float kLargeStaggerDuration = 0.6f;

	// PlayAnimation()でblendDurationSecondsを省略した場合に使う既定値。
	// ModelAnimatorComponent側の初期値(0.15秒)と合わせている。
	static constexpr float kDefaultAnimationBlendDuration = 0.15f;

	// --- 戦闘データ ---
	AttackMoveData currentAttack_;
	EvadeMoveData currentEvade_;
	GuardMoveData currentGuard_;

	// Evade/Guardの基本データ(デバッグ用テーブルから読み込んだもの)。
	// currentEvade_/currentGuard_は「今回発動する際の実際の値」
	// (Evadeは方向がそのつど変わる)、こちらは「その元になる基本データ」
	// という役割分担。
	EvadeMoveData baseEvadeData_;
	GuardMoveData baseGuardData_;

	// コンボ攻撃: 何段目か(0始まり)と、各段の技データテーブル。
	// 各要素の具体的な数値(モーション時間・踏み込み距離等)は別途詰める
	// (現状は全段ともAttackMoveDataのデフォルト値+仮のanimationNameのまま)。
	int comboIndex_ = 0;
	ComboAttackTable comboAttacks_;

	// --- Stateインスタンス (メモリ断片化を防ぐため実体をメンバで持つ) ---
	StateNone    stateNone_;
	StateAttack  stateAttack_;
	StateEvade   stateEvade_;
	StateGuard   stateGuard_;
	StateStagger stateStagger_;

	// 共通StateMachine基盤(遷移ロジック・現在Stateの保持)
	StateMachine<PlayerStatusController, IPlayerState> stateMachine_;
};