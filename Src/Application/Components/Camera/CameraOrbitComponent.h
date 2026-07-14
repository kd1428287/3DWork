#pragma once
#include <algorithm>
#include "../Transform/TransformComponent.h"

// ============================================================
// マウスの移動量を、対象を中心とした軌道回転(ヨー/ピッチ)として
// 蓄積するコンポーネント。
//
// 「マウスがどれだけ動いたか」を軌道角度に変換して溜め込むことだけが
// 責務で、実際にカメラの位置へどう反映するかは持たない。
// CameraFollowComponentが、対象の向き(ICameraTarget::GetTargetRotation())の
// 代わりにこちらのGetOrbitRotation()を使うことで、「対象がどちらを
// 向いていても、マウスで自由に見回せる」三人称カメラになる
// (詳細はCameraFollowComponent::PostUpdate()参照)。
//
// PlayerInputComponentと同じ理由で、ハードウェア入力(KdInputManager)は
// 直接読まない。生のマウス移動量の注入は毎フレームInputSystem側から
// SetLookDelta()経由で行う。
// ============================================================
class CameraOrbitComponent : public ComponentBase {
public:
	explicit CameraOrbitComponent(GameObject* owner) : ComponentBase(owner) {}

	// 外部(InputSystem)から、このフレームのマウス移動量(ピクセル/フレーム)を注入する。
	void SetLookDelta(const Math::Vector2& delta) { lookDelta_ = delta; }

	void Update(float /*deltaTime*/) override {
		yaw_ += lookDelta_.x * sensitivity_;
		pitch_ += lookDelta_.y * sensitivity_ * (invertPitch_ ? -1.0f : 1.0f);

		// 真上/真下を通り越して反転しないようクランプする(ラジアン)。
		pitch_ = std::clamp(pitch_, minPitch_, maxPitch_);

		// Look軸自体は毎フレームInputSystem側で上書きされる想定だが、
		// 万一注入が途切れた場合に前フレームの値が残らないよう明示的にクリアする。
		lookDelta_ = Math::Vector2::Zero;
	}

	// 現在の軌道角度を、オフセット回転用のQuaternionとして返す。
	Math::Quaternion GetOrbitRotation() const {
		return Math::Quaternion::CreateFromYawPitchRoll(yaw_, pitch_, 0.0f);
	}

	// ピクセル移動量→ラジアンへの変換係数。値が大きいほど感度が高い。
	void SetSensitivity(float sensitivity) { sensitivity_ = sensitivity; }

	// 見上げ/見下ろしの限界(ラジアン)。真上・真下の直角(±π/2)より
	// 少し手前に制限しておくと、カメラが反転して見える事故を防げる。
	void SetPitchLimits(float minPitch, float maxPitch) {
		minPitch_ = minPitch;
		maxPitch_ = maxPitch;
	}

	// マウスを上に動かした時に見上げる/見下ろすの向きを反転したい場合。
	// ※ pitchの符号とCreateFromYawPitchRollの回転規約はプロジェクトの
	//   座標系依存のため、実機で上下が逆に感じたらこちらで調整すること。
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
