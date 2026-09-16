#pragma once

// 何らかのコマンドを先行入力として一定時間だけ保持するバッファ

// 先行入力バッファの1エントリ。
template <typename TCommand>
struct BufferedInput
{
	TCommand command;
	float timeRemaining; // 先行入力が有効な残り時間
	Math::Vector3 direction = Math::Vector3::Zero;
};

template <typename TCommand>
class CharacterInputBufferComponent : public ComponentBase
{
public:
	explicit CharacterInputBufferComponent(GameObject* owner) : ComponentBase(owner) {}

	// 先行入力バッファの有効期限を減算する。
	void PostUpdate(float deltaTime) override
	{
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

	// コマンドが発生した瞬間、呼び出し側から呼ぶ
	void PushCommand(TCommand command, const Math::Vector3& direction, float bufferTime = 0.2f)
	{
		Math::Vector3 dir = direction;
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
	bool HasCommand(TCommand command) const {
		for (const auto& input : inputBuffer_) {
			if (input.command == command) return true;
		}
		return false;
	}

	bool ConsumeCommand(TCommand command) {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end(); ++it) {
			if (it->command == command) {
				inputBuffer_.erase(it); // 消費したためバッファから消す
				return true;
			}
		}
		return false;
	}

	// 方向スナップショットも合わせて取り出したい場合(Evade等)はこちら。
	bool ConsumeCommand(TCommand command, Math::Vector3& outDirection) {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end(); ++it) {
			if (it->command == command) {
				outDirection = it->direction;
				inputBuffer_.erase(it);
				return true;
			}
		}
		return false;
	}

private:
	std::vector<BufferedInput<TCommand>> inputBuffer_;

	static constexpr float kDirectionEpsilon = 1e-6f;
};