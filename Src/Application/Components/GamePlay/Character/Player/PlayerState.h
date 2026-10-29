#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "Application/Definitions/Character/Common/StateMachine/StateMachine.h"

// 前方宣言
struct PlayerStateContext;

// ============================================================
// IPlayerState
// 共通StateMachine基盤(IState<PlayerStateContext>)に、
// プレイヤー戦闘特有の問い合わせ(具体フェーズ・遷移可否・ジャスト判定)を
// 追加した軽量Stateパターンのインターフェース。GameObjectにはアタッチせず、
// PlayerStatusController内部でインスタンスとして保持される。
// ============================================================
class IPlayerState : public IState<PlayerStateContext> {
public:
	// 現在の具体的なフェーズと経過時間を返す
	virtual CombatState GetDetailedState() const = 0;
	virtual float GetElapsed() const = 0;

	// 次のアクションへの遷移可否（デフォルトは不可）。
	// Guardもここに揃えることで、HandleActionInput側にCombatStateの
	// ハードコード判定を書かずに済むようにする。
	virtual bool CanStartAttack(const PlayerStateContext* ctx) const { return false; }
	virtual bool CanStartEvade(const PlayerStateContext* ctx) const { return false; }
	virtual bool CanStartGuard(const PlayerStateContext* ctx) const { return false; }
	virtual bool CanStartMove(const PlayerStateContext* ctx) const { return false; }

	// ガードキーを離した際に即座に解除してよいか(デフォルトは常に許可)。
	// StateGuardがパリィ成功演出中だけfalseを返し、演出を強制的に
	// 見せ切る(HandleActionInput参照)。
	virtual bool CanReleaseGuard(const PlayerStateContext* ctx) const { return true; }

	// ジャスト判定の問い合わせ（該当するStateのみがオーバーライドしてtrueを返す）
	virtual bool IsInvincible(const PlayerStateContext* ctx) const { return false; }
	virtual bool IsInJustEvadeWindow(const PlayerStateContext* ctx) const { return false; }
	virtual bool IsInParryWindow(const PlayerStateContext* ctx) const { return false; }
};

// 自分用のデータ構造体をメンバに持つStateの共通基底。
// データはSetData()で注入し、Stateはdata_を直接参照する。
template<class TData>
class DataState : public IPlayerState {
public:
	void SetData(const TData& data) { data_ = data; }
	const TData& GetData() const { return data_; }

protected:
	TData data_;
};

class StateNone : public IPlayerState {
public:
	// Noneに戻った瞬間、現在の入力状態に合わせてIdle/Walk/Runへ
	// アニメーションを同期し直す(行動中ずっと同じ入力のままだった場合の
	// 固まり対策。詳細はPlayerStatusController::RefreshMovementAnimation参照)。
	void Enter(PlayerStateContext* ctx) override;

	CombatState GetDetailedState() const override { return CombatState::None; }
	float GetElapsed() const override { return 0.0f; }

	// None状態からは何でも出せる
	bool CanStartAttack(const PlayerStateContext* ctx) const override { return true; }
	bool CanStartEvade(const PlayerStateContext* ctx) const override { return true; }
	bool CanStartGuard(const PlayerStateContext* ctx) const override { return true; }
};

class StateAttack : public DataState<AttackStateData> {
public:
	// 次のEnter()で掛けるダメージ倍率(通常攻撃は1.0、チャージ攻撃は溜め量で決まる)。
	void Setup(float damageScale) { damageScale_ = damageScale; }

	void Enter(PlayerStateContext* ctx) override;
	void Update(PlayerStateContext* ctx, float deltaTime) override;
	void Exit(PlayerStateContext* ctx) override;

	CombatState GetDetailedState() const override { return phase_; }
	float GetElapsed() const override { return elapsed_; }

	// Recovery中の一定タイミングを過ぎたら、回避だけでなく次の攻撃
	// (コンボ)によるキャンセルも許可する。
	bool CanStartAttack(const PlayerStateContext* ctx) const override;
	bool CanStartEvade(const PlayerStateContext* ctx) const override;
	bool CanStartGuard(const PlayerStateContext* ctx) const override;
	bool CanStartMove(const PlayerStateContext* ctx) const override;

private:
	CombatState phase_ = CombatState::AttackWindup;
	float elapsed_ = 0.0f;
	float damageScale_ = 1.0f;

	// ルートモーション踏み込みの距離スケールをクランプする範囲。
	// 敵が遠すぎる/近すぎる場合に不自然な速度で動くのを防ぐ安全弁で、
	// 技ごとの調整値ではないためここに定数として持つ。
	static constexpr float kMinRootMotionWarpScale = 0.1f;
	static constexpr float kMaxRootMotionWarpScale = 1.8f;
};


class StateEvade : public DataState<EvadeData> {
public:
	// 入力方向(実行時入力)。静的な回避データはSetData()で渡す。
	void Setup(const Math::Vector3& direction) { direction_ = direction; }

	void Enter(PlayerStateContext* ctx) override;
	void Update(PlayerStateContext* ctx, float deltaTime) override;
	void Exit(PlayerStateContext* ctx) override;

	CombatState GetDetailedState() const override { return phase_; }
	float GetElapsed() const override { return elapsed_; }

	bool IsInJustEvadeWindow(const PlayerStateContext* ctx) const override;
	bool IsInvincible(const PlayerStateContext* ctx) const override; // 追加

private:
	CombatState phase_ = CombatState::Evade;
	float elapsed_ = 0.0f;
	Math::Vector3 direction_ = Math::Vector3::Zero;
};


class StateGuard : public DataState<GuardData> {
public:
	void Enter(PlayerStateContext* ctx) override;
	void Update(PlayerStateContext* ctx, float deltaTime) override;
	void Exit(PlayerStateContext* ctx) override;

	CombatState GetDetailedState() const override { return CombatState::Guard; }
	float GetElapsed() const override { return elapsed_; }

	bool IsInParryWindow(const PlayerStateContext* ctx) const override;

	// パリィ成功演出中はガードキーを離しても解除させない(強制的に見せ切る)。
	bool CanReleaseGuard(const PlayerStateContext* ctx) const override;

	bool CanStartAttack(const PlayerStateContext* ctx) const override;
	bool CanStartEvade(const PlayerStateContext* ctx) const override;
	bool CanStartGuard(const PlayerStateContext* ctx) const override;
	bool CanStartMove(const PlayerStateContext* ctx) const override;

	// HitReactionComponent(IHitReactionQuery経由、PlayerStatusController::
	// NotifyParrySuccess/NotifyGuardHit)から呼ばれる通知。
	void NotifyParrySuccess(PlayerStateContext* ctx);
	void NotifyGuardHit(PlayerStateContext* ctx);

	// ガードキー解放時にPlayerStatusController::HandleActionInputから
	// 呼ばれる解除要求。ここではまだNoneへ遷移せず、終了アニメーション
	// (endAnimationName)の再生を開始するだけ。endDuration経過後、
	// Update()側が自律的にChangeStateToNone()する(StateAttackの
	// Recovery終了と同じ考え方)。
	void RequestRelease(PlayerStateContext* ctx);

private:
	// パリィ判定はGuard内部のサブフェーズとして扱う。
	// ParrySuccessは時間経過ではなくNotifyParrySuccess()による外部通知で
	// 開始し、parrySuccessDuration秒経過で自動的にNormalBlockへ戻る
	// (GetGuardPhase()参照。CombatStateやControllerには手を入れず
	//  Guard内部だけで完結させる、という元々の方針を踏襲)。
	enum class GuardPhase { JustWindow, NormalBlock, ParrySuccess, Release };
	GuardPhase GetGuardPhase(const PlayerStateContext* ctx) const;

	// パリィ成功/ガードヒットの単発リアクション再生が終わった後、
	// 継続姿勢のLoopアニメーションへ明示的に再生し直すためのヘルパー。
	void ResumeLoopAnimation(PlayerStateContext* ctx);

	float elapsed_ = 0.0f;

	// 構え動作(animationName)の単発再生が終わり、継続姿勢のLoop
	// アニメーションへ切り替え済みかどうか。
	bool hasEnteredLoop_ = false;

	bool parrySucceeded_ = false;
	float parrySuccessElapsed_ = 0.0f;

	// パリィ成功と異なりゲームプレイ上の制約(CanReleaseGuard/CanStartAttack)
	// には影響させない、ガードヒットのアニメーション再生専用のフラグ。
	bool isReactingToGuardHit_ = false;
	float guardHitElapsed_ = 0.0f;

	// ガード解除要求を受けてから、終了アニメーション(endAnimationName)を
	// 再生し終えるまでの間だけtrue。この間はStart/Loop/各リアクションの
	// 更新を止め、endDuration経過を待つだけの状態になる。
	bool isReleasing_ = false;
	float releaseElapsed_ = 0.0f;
};


// Attack+Guard同時押し中の溜め状態。発射(ReleaseCharge)はControllerが行う。
class StateCharge : public DataState<ChargeData> {
public:
	void Enter(PlayerStateContext* ctx) override;
	void Update(PlayerStateContext* ctx, float deltaTime) override;
	void Exit(PlayerStateContext* ctx) override;

	CombatState GetDetailedState() const override { return CombatState::Charge; }
	float GetElapsed() const override { return elapsed_; }

	// 溜め中は回避でキャンセルできる。
	bool CanStartEvade(const PlayerStateContext* ctx) const override { return true; }

private:
	float elapsed_ = 0.0f;
};


class StateStagger : public DataState<StaggerData> {
public:
	// 怯み/ダウンの種別だけを受け取る。Reaction/Recoveryの秒数は
	// StaggerData(small/large)側の値を使う(Enter()で読み込む)。
	void Setup(bool isLarge) { isLarge_ = isLarge; }

	void Enter(PlayerStateContext* ctx) override;
	void Update(PlayerStateContext* ctx, float deltaTime) override;
	void Exit(PlayerStateContext* ctx) override;

	CombatState GetDetailedState() const override {
		return isLarge_ ? CombatState::StaggerLarge : CombatState::StaggerSmall;
	}
	float GetElapsed() const override { return elapsed_; }

	// Recovery中、一定タイミングを過ぎたらEvade/移動によるキャンセルを許可する
	// (StateAttack::CanStartEvade/CanStartMoveと同じ考え方)。
	bool CanStartEvade(const PlayerStateContext* ctx) const override;
	bool CanStartMove(const PlayerStateContext* ctx) const override;

private:
	enum class Phase { Reaction, Recovery };
	Phase phase_ = Phase::Reaction;

	// isLarge_に応じてsmall/largeどちらのデータを見るかをまとめるヘルパー。
	const StaggerPhaseData& GetPhaseData(const PlayerStateContext* ctx) const;

	bool isLarge_ = false;
	float reactionDuration_ = 0.0f;
	float elapsed_ = 0.0f;
};


// 死亡状態。CanStart*は全てfalseのままで、入力でも移動キャンセルでも抜けられない終端State。
class StateDead : public DataState<DeadData> {
public:
	void Enter(PlayerStateContext* ctx) override;
	void Update(PlayerStateContext* ctx, float deltaTime) override;
	void Exit(PlayerStateContext* ctx) override;

	CombatState GetDetailedState() const override { return CombatState::Dead; }
	float GetElapsed() const override { return elapsed_; }

	// 死体への追加被弾を無効化する。
	bool IsInvincible(const PlayerStateContext* ctx) const override { return true; }

private:
	float elapsed_ = 0.0f;
	bool reloadRequested_ = false;
};