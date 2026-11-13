#pragma once
#include "Application/Effect/Shell/WindShellDispatcher.h"

// プレハブJSONで指定する設定。未指定のキーは初期値になる。
struct WindShellConfig {
	std::string preset = "EnemyAura";
	Math::Vector3 localAxis = { 0,-1,0 };// Math::Vector3::Forward;	// 風の軸(このオブジェクトのローカル)
	float axisOffset = 0.0f;
	bool useVelocity = false;	// trueなら速く動く間は移動方向を軸にする
	float speedThreshold = 1.0f;
	float axisFollow = 20.0f;
	bool autoPlay = false;
};

class WindShellComponent : public ComponentBase {
public:
	enum class AxisMode {
		LocalAxis,	// 自オブジェクトのローカル軸(SetLocalAxis)を向きとする
		Velocity,	// 速く動いている間は移動方向、遅いときはLocalAxis
	};

	explicit WindShellComponent(GameObject* owner, std::string presetName = "EnemyAura")
		: ComponentBase(owner), presetName_(std::move(presetName)) {}

	void Awake() override {
		selfTransform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void PostUpdate(float deltaTime) override {
		// 自動再生は最初のPostUpdateで行う(Awake時点ではディスパッチャーが購読前の可能性があるため)
		if (autoPlayPending_) {
			autoPlayPending_ = false;
			Play();
		}

		if (!wind_.IsActive() || selfTransform_ == nullptr) { return; }

		const Math::Vector3 pos = selfTransform_->GetPosition();
		UpdateAxis(pos, deltaTime);
		prevPos_ = pos;
		hasPrev_ = true;

		wind_.Update(pos + axis_ * axisOffset_, axis_, progress_);
	}

	void OnDestroy() override { Stop(); }

	void SetConfig(const WindShellConfig& c) {
		presetName_ = c.preset;
		localAxis_ = c.localAxis;
		axisOffset_ = c.axisOffset;
		axisMode_ = c.useVelocity ? AxisMode::Velocity : AxisMode::LocalAxis;
		speedThreshold_ = c.speedThreshold;
		axisFollow_ = c.axisFollow;
		autoPlayPending_ = c.autoPlay;
	}

	// --- 操作 ---------------------------------------------------

	// 発生させる。再生中なら即時に作り直す
	void Play() {
		if (selfTransform_ == nullptr) { return; }

		const Math::Vector3 pos = selfTransform_->GetPosition();
		axis_ = LocalAxisWorld();
		prevPos_ = pos;
		hasPrev_ = false;

		wind_.Start(GetOwner()->GetSceneEventBus(), presetName_, pos + axis_ * axisOffset_, axis_);
	}

	// 消去(ディゾルブ)へ移す。以後の更新は止まり、風はその場で消える
	void Stop() { wind_.Stop(); }

	bool IsPlaying() const { return wind_.IsActive(); }

	// チャージ量などの進行度(0〜1)
	void SetProgress(float progress) { progress_ = std::clamp(progress, 0.0f, 1.0f); }

	// --- 設定 ---------------------------------------------------

	void SetPreset(std::string presetName) { presetName_ = std::move(presetName); }
	void SetAxisMode(AxisMode mode) { axisMode_ = mode; }
	void SetLocalAxis(const Math::Vector3& axis) { localAxis_ = axis; }
	void SetAxisOffset(float offset) { axisOffset_ = offset; }	// 軸方向への中心のずらし(m)
	void SetSpeedThreshold(float speed) { speedThreshold_ = speed; }	// Velocityモードの切り替え速度(m/s)
	void SetAxisFollow(float follow) { axisFollow_ = follow; }	// 軸の追従の速さ(大きいほど速い)
	void SetAutoPlay(bool enable) { autoPlayPending_ = enable; }

private:
	Math::Vector3 LocalAxisWorld() const {
		Math::Vector3 axis = Math::Vector3::Transform(localAxis_, selfTransform_->GetRotation());
		axis.Normalize();
		return axis;
	}

	void UpdateAxis(const Math::Vector3& pos, float deltaTime) {
		Math::Vector3 target = LocalAxisWorld();

		// ヒットストップ中(dt≒0)は速度を計算せず、前回の軸を保つ
		if (axisMode_ == AxisMode::Velocity && hasPrev_ && deltaTime > 1e-5f) {
			const Math::Vector3 velocity = (pos - prevPos_) / deltaTime;
			if (velocity.LengthSquared() > speedThreshold_ * speedThreshold_) {
				target = velocity;
				target.Normalize();
			}
		}

		// フレームレートに依存しない平滑化。真逆で打ち消し合った場合は目標へ切り替える
		const float t = 1.0f - std::exp(-axisFollow_ * deltaTime);
		Math::Vector3 next = Math::Vector3::Lerp(axis_, target, t);
		if (next.LengthSquared() < 1e-6f) { next = target; }
		next.Normalize();
		axis_ = next;
	}

	std::string presetName_;
	WindShellHandle wind_;
	TransformComponent* selfTransform_ = nullptr;

	AxisMode axisMode_ = AxisMode::LocalAxis;
	Math::Vector3 localAxis_ = Math::Vector3::Forward;
	float axisOffset_ = 0.0f;
	float speedThreshold_ = 1.0f;
	float axisFollow_ = 20.0f;

	Math::Vector3 axis_ = Math::Vector3::Up;
	Math::Vector3 prevPos_ = Math::Vector3::Zero;
	bool hasPrev_ = false;
	float progress_ = 0.0f;
	bool autoPlayPending_ = false;
};