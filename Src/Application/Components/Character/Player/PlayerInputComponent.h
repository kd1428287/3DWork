#pragma once
#include <vector>
#include "PlayerCombatTypes.h"
#include "../../Movement/IMovementSource.h"

#include "../../Camera/CameraComponent.h"

// 先行入力バッファの1エントリ。Attack/Evadeのようなタップ入力のみを扱う。
// パリィはGuard開始直後の時間窓として表現するため、ここには含まれない。
struct BufferedInput
{
	ActionCommand command;
	float timeRemaining; // 先行入力が有効な残り時間（秒、例: 0.2秒）

	// このコマンドが積まれた瞬間の移動入力方向(正規化済み)のスナップショット。
	// 方向を使わないコマンド(Attack等)では単に無視される。
	// バッファ滞留中に入力方向が変わっても、積まれた瞬間の意図がブレないようにする。
	Math::Vector3 direction = Math::Vector3::Zero;
};

class PlayerInputComponent : public ComponentBase, public IMovementSource {
public:
	explicit PlayerInputComponent(GameObject* owner) : ComponentBase(owner) {}

	// GameObjectのUpdate、またはPreUpdateで毎フレーム呼び出し、
	// 先行入力バッファの有効期限（timeRemaining）を deltaTime 分減算する
	void Update(float deltaTime) override {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end();) {
			it->timeRemaining -= deltaTime;
			if (it->timeRemaining <= 0.0f) {
				it = inputBuffer_.erase(it); // 有効期限切れは削除
			}
			else {
				++it;
			}
		}
	}

	// --- 外部（InputSystemなど）から毎フレーム入力を注入する関数 ---

	void SetMoveDirection(Math::Vector3 direction) {
		moveDirection_ = direction;
	}

	// Guard/Dashは「押されている間ずっと」の継続状態。
	// PushCommand(先行入力バッファ)とは性質が違うため、都度上書きするだけにする。
	void SetGuardHeld(bool held) { guardHeld_ = held; }
	void SetDashHeld(bool held) { dashHeld_ = held; }

	// Lockは押した瞬間だけ意味を持つ単発入力だが、Attack/Evadeのように
	// 「一定時間分の猶予を持たせて後から消費される」必要が無い
	// (ロックオンの成立/解除に先行入力の概念はそぐわない)。
	// そのためPushCommand(先行入力バッファ)には積まず、単純に
	// 「このフレーム押されたか」を保持するだけのフラグとして扱う。
	// InputSystem::Update()からIsPress("Lock")の瞬間にSetLockPressed()が
	// 呼ばれ、PlayerStatusController::HandleActionInput()が同じフレーム内で
	// ConsumeLockPressed()を呼んで読み捨てる想定(1フレーム内で
	// セット→消費が完結するため、Update()側での寿命管理は不要)。
	void SetLockPressed() { lockPressed_ = true; }

	// ボタンが押された瞬間に呼ばれる（バッファにキューイング）
	// この瞬間のmoveDirection_を正規化してスナップショットしておく
	// (Evadeの回避方向など、方向を伴うコマンド向け)。
	void PushCommand(ActionCommand command, float bufferTime = 0.2f) {
		Math::Vector3 dir = moveDirection_;
		if (dir.LengthSquared() > kDirectionEpsilon) {
			dir.Normalize();
		}
		else {
			dir = Math::Vector3::Zero;
		}
		inputBuffer_.push_back({ command, bufferTime, dir });
	}

	// --- 外部（PlayerStatusControllerなど）が先行入力を確認/消費する関数 ---

	// 覗き見用。実行可能かどうかを先に判定してからConsumeCommand()を
	// 呼びたい場合に使う。これを経由せずいきなりConsumeCommand()を呼ぶと、
	// まだ猶予が残っている入力を実行不可なタイミングで誤って消費してしまう。
	bool HasCommand(ActionCommand command) const {
		for (const auto& input : inputBuffer_) {
			if (input.command == command) return true;
		}
		return false;
	}

	bool ConsumeCommand(ActionCommand command) {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end(); ++it) {
			if (it->command == command) {
				inputBuffer_.erase(it); // 消費したためバッファから消す
				return true;
			}
		}
		return false;
	}

	// 方向スナップショットも合わせて取り出したい場合(Evade等)はこちら。
	bool ConsumeCommand(ActionCommand command, Math::Vector3& outDirection) {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end(); ++it) {
			if (it->command == command) {
				outDirection = it->direction;
				inputBuffer_.erase(it);
				return true;
			}
		}
		return false;
	}

	bool IsGuardHeld() const { return guardHeld_; }
	bool IsDashHeld() const { return dashHeld_; }

	// このフレームLockが押されていたかを取り出し、同時にフラグを消費(false)する。
	// HasCommand()に相当する「覗き見だけ」の版は用意していない。Lockは
	// CanStart*系のような実行可否判定を挟まず常に処理してよい入力のため、
	// ConsumeCommand()相当の一発だけで足りる。
	bool ConsumeLockPressed() {
		bool pressed = lockPressed_;
		lockPressed_ = false;
		return pressed;
	}

	// 移動方向とDashキーの状態から、今フレームの移動意思をMovementStateとして
	// 解決する。キーボード操作前提のため、アナログの傾き量ではなく
	// Dashキーが押されているか否かでWalk/Runを切り替える。
	MovementState GetDesiredMovementState() const {
		if (moveDirection_.LengthSquared() <= 0.0f) return MovementState::Stand;
		return dashHeld_ ? MovementState::Run : MovementState::Walk;
	}

	// IMovementSourceの実装
	Math::Vector3 GetDesiredVelocity() override {
		return moveDirection_;
	}

	// moveDirection_の読み取り専用アクセサ。GetDesiredVelocity()は
	// IMovementSource側の都合でconstにできないため、const参照からも
	// 呼べる版を別途用意する(PlayerStatusController::HandleMovementInput
	// がWalk中の前後左右判定に使う。ClassifyEvadeDirection参照)。
	Math::Vector3 GetMoveDirection() const { return moveDirection_; }

private:
	Math::Vector3 moveDirection_;
	bool dashHeld_ = false;
	bool guardHeld_ = false;
	bool lockPressed_ = false; // このフレームLockが押されたか(消費されるまで保持)
	std::vector<BufferedInput> inputBuffer_; // 先行入力バッファ(Attack/Evadeのみ)
};