#pragma once
#include <vector>
#include "../Transform/IMovementSource.h"

enum class ActionCommand {
	Attack,
	Guard,  // 押しっぱなしでガード
	Parry,  // 押した瞬間にトリガーされる判定用
	Evade
};

struct BufferedInput {
	ActionCommand command;
	float timeRemaining; // 先行入力が有効な残り時間（秒、例: 0.2秒）
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
	void SetMoveDirection(Math::Vector3 direction) { moveDirection_ = direction; }

	// ボタンが押された瞬間に呼ばれる（バッファにキューイング）
	void PushCommand(ActionCommand command, float bufferTime = 0.2f) {
		inputBuffer_.push_back({ command, bufferTime });
	}

	// --- 外部（StateやCombatComponentなど）が「先行入力があるか」確認し、消費する関数 ---
	bool ConsumeCommand(ActionCommand command) {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end(); ++it) {
			if (it->command == command) {
				inputBuffer_.erase(it); // 消費したためバッファから消す
				return true;
			}
		}
		return false;
	}

	// IMovementSourceの実装
	Math::Vector3 GetDesiredVelocity(float /*deltaTime*/) override {
		return moveDirection_;
	}

private:
	Math::Vector3 moveDirection_;
	std::vector<BufferedInput> inputBuffer_; // 先行入力バッファ
};