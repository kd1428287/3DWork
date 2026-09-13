#pragma once
#include <algorithm>
#include "../Transform/TransformComponent.h"

// マウス移動量を対象を中心とした軌道回転移動とする
class CameraOrbitComponent : public ComponentBase {
public:
	explicit CameraOrbitComponent(GameObject* owner) : ComponentBase(owner) {}

	// 外部からマウス移動量を注入する。
	void SetLookDelta(const Math::Vector2& delta) { lookDelta_ = delta; }

	void Update(float /*deltaTime*/) override {
		yaw_ += lookDelta_.x * sensitivity_;
		pitch_ += lookDelta_.y * sensitivity_ * (invertPitch_ ? -1.0f : 1.0f);

		// 真上/真下を通り越して反転しないようクランプする
		pitch_ = std::clamp(pitch_, minPitch_, maxPitch_);

		lookDelta_ = Math::Vector2::Zero;
	}

	// 現在の軌道角度を、オフセット回転用のQuaternionとして返す。
	Math::Quaternion GetOrbitRotation() const {
		return Math::Quaternion::CreateFromYawPitchRoll(yaw_, pitch_, 0.0f);
	}

	// ピクセル移動量→ラジアンへの変換係数。値が大きいほど感度が高い。
	void SetSensitivity(float sensitivity) { sensitivity_ = sensitivity; }

	// 見上げ/見下ろしの限界(ラジアン)
	void SetPitchLimits(float minPitch, float maxPitch) {
		minPitch_ = minPitch;
		maxPitch_ = maxPitch;
	}

	// マウスを上に動かした時に見上げる/見下ろすの向きを反転したい場合
	void SetInvertPitch(bool invert) { invertPitch_ = invert; }

	float GetYaw() const { return yaw_; }
	float GetPitch() const { return pitch_; }

	// カットシーン等でカメラの向きを強制的に合わせたい場合に使う。
	void SetYawPitch(float yaw, float pitch) {
		yaw_ = yaw;
		pitch_ = std::clamp(pitch, minPitch_, maxPitch_);
	}

private:
	Math::Vector2 lookDelta_ = Math::Vector2::Zero;

	float yaw_ = 0.0f;
	float pitch_ = 0.0f;
	float sensitivity_ = 0.0025f;

	float minPitch_ = -1.2f; // 約-69度
	float maxPitch_ = 1.2f;  // 約+69度
	bool invertPitch_ = true;
};