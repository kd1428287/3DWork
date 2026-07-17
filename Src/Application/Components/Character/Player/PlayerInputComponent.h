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

private:
	Math::Vector3 moveDirection_;
	bool dashHeld_ = false;
	bool guardHeld_ = false;
	std::vector<BufferedInput> inputBuffer_; // 先行入力バッファ(Attack/Evadeのみ)
};