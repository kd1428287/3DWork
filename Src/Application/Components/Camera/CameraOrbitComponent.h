#pragma once
#include <algorithm>
#include <cmath>
#include "../Transform/TransformComponent.h"
#include "../../Core/SceneContext.h"

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
//
// --- ロックオン対応 -------------------------------------------------
// SceneContext::lockedTarget(PlayerLockOnComponentが更新する、シーンに
// 1つだけの既知のロック対象。SceneContext.h参照)が有効な間は、
// 通常のマウス軌道回転の蓄積を止め、代わりにロック対象の方向へ
// yaw/pitchを滑らかに近づける。これによりカメラはPlayerLockOnComponent
// という型を一切知らないまま、ロック対象を向くようになる。
//
// 自分(カメラ)の位置はCameraFollowComponent::PostUpdate()が「このUpdate()で
// 計算したyaw/pitch」を基に前フレーム決定するため、ここで使うtransform_の
// 位置は1フレーム遅れたカメラ位置になる。既存コード内の他の1フレーム遅延
// (例: ClassifyEvadeDirectionが前フレームの向きを使うのと同様)と同程度の
// 誤差であり、実害があるほどではないという判断のもとで許容している。
// ============================================================
class CameraOrbitComponent : public ComponentBase {
public:
	explicit CameraOrbitComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		// ロック中にtarget方向を計算する際、自分(カメラ)の現在位置の
		// 参照に使う(UpdateLockedOrbit()参照)。
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// 外部(InputSystem)から、このフレームのマウス移動量(ピクセル/フレーム)を注入する。
	void SetLookDelta(const Math::Vector2& delta) { lookDelta_ = delta; }

	void Update(float deltaTime) override {
		// ロック中はSceneContext::lockedTargetの方向へ向くことを優先し、
		// 通常のマウス軌道回転の蓄積はしない(下のUpdateLockedOrbit参照)。
		if (const SceneContext* context = GetOwner()->GetContext()) {
			if (GameObject* target = context->lockedTarget.Resolve()) {
				UpdateLockedOrbit(target, deltaTime);
				// ロック解除した瞬間に、溜まっていたマウス移動量分だけ
				// 視点が急に飛ぶことがないよう、蓄積せず読み捨てる。
				lookDelta_ = Math::Vector2::Zero;
				return;
			}
		}

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

	// ロック中、対象方向へ向き直る速さ(ラジアン/秒)。値が大きいほど
	// 瞬時に近い向き直りになる。
	void SetLockOnTurnSpeed(float speed) { lockOnTurnSpeed_ = speed; }

	float GetYaw() const { return yaw_; }
	float GetPitch() const { return pitch_; }

	// カットシーン等でカメラの向きを強制的に合わせたい場合に使う。
	void SetYawPitch(float yaw, float pitch) {
		yaw_ = yaw;
		pitch_ = std::clamp(pitch, minPitch_, maxPitch_);
	}

private:
	// ロック対象の方向へ、yaw/pitchを一定速度で近づける。
	// 瞬時に向き直すと不自然なため、ApproachAngle()で少しずつ回す
	// (CameraCollisionComponentのpullOutSpeed_と同じ「なめらかに近づける」考え方)。
	void UpdateLockedOrbit(GameObject* target, float deltaTime) {
		if (transform_ == nullptr) return;
		TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return;

		const Math::Vector3 dir = targetTransform->GetPosition() - transform_->GetPosition();
		const float horizontalLenSq = dir.x * dir.x + dir.z * dir.z;
		if (horizontalLenSq <= kMinDirectionLengthSq) return; // 対象がほぼ真上/真下、あるいは自分と同じ位置

		// 【要確認】+Z前方の左手系を想定したyaw/pitch算出。
		// PlayerStatusController::FaceTowardsやClassifyEvadeDirectionと
		// 同じ座標系前提の確認が必要な箇所。実機で左右/上下が逆に見える
		// 場合はここの符号を調整すること。
		const float horizontalLen = std::sqrt(horizontalLenSq);
		const float desiredYaw = std::atan2(dir.x, dir.z);
		const float desiredPitch = std::atan2(dir.y, horizontalLen);

		const float maxDelta = lockOnTurnSpeed_ * deltaTime;
		yaw_ = ApproachAngle(yaw_, desiredYaw, maxDelta);
		pitch_ = std::clamp(ApproachAngle(pitch_, desiredPitch, maxDelta), minPitch_, maxPitch_);
	}

	// 角度(ラジアン)をcurrentからtargetへ、1フレームあたり最大maxDeltaだけ
	// 近づける。±πの境界をまたぐ場合でも遠回りしないよう、差分を
	// sin/cos経由で(-π, π]に正規化してから使う。
	static float ApproachAngle(float current, float target, float maxDelta) {
		const float diff = std::atan2(std::sin(target - current), std::cos(target - current));
		if (diff > maxDelta) return current + maxDelta;
		if (diff < -maxDelta) return current - maxDelta;
		return current + diff;
	}

	TransformComponent* transform_ = nullptr;

	Math::Vector2 lookDelta_ = Math::Vector2::Zero;

	float yaw_ = 0.0f;
	float pitch_ = 0.0f;
	float sensitivity_ = 0.0025f;

	float minPitch_ = -1.2f; // 約-69度
	float maxPitch_ = 1.2f;  // 約+69度
	bool invertPitch_ = true;

	// ロック中にyaw/pitchを対象方向へ近づける速さ(ラジアン/秒、仮の値)。
	float lockOnTurnSpeed_ = 6.0f;

	// ロック対象方向の水平成分がこれ以下(ほぼ真上/真下、または自分と同じ
	// 位置)の場合は向きを更新しない、という閾値。PlayerCombatTypes.hの
	// kDirectionEpsilonと同じ役割だが、カメラ側はPlayerモジュールの型に
	// 依存したくないため独自に持つ。
	static constexpr float kMinDirectionLengthSq = 1e-6f;
};