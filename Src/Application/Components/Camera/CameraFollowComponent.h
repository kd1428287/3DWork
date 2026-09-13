#pragma once
#include <cmath>
#include "ICameraTarget.h"
#include "../Transform/TransformComponent.h"
#include "../Camera/CameraTargetComponent.h"
#include "../../Core/Handle.h"
#include "../../Core/SceneContext.h"
#include "CameraOrbitComponent.h"

// カメラの追従ロジックだけを持つ

// --- ロックオン対応 -------------------------------------------------
// SceneContext::lockedTarget(PlayerLockOnComponentが更新する、シーンに
// 1つだけの既知のロック対象。SceneContext.h参照)が有効な間は、
// 「位置」も「向き」も対象基準で毎フレーム計算し直す
// (以前はここが2段階に分かれていて不具合の元になっていた。下記参照)。
//
// 【経緯】
// 1段階目: 位置も向きも同じ回転(カメラ→対象の向き)で決めていたところ、
//   カメラ・プレイヤー・対象が一直線に並び、プレイヤーが対象を隠す
//   不具合が発生した。
// 2段階目: 位置をロック開始時点のマウス操作の軌道のまま「凍結」させ、
//   向きだけを対象へ向けるようにしたところ、位置はワールド空間で固定の
//   オフセットベクトルのままプレイヤーに追従するだけなので、プレイヤーが
//   横に動くと「固定されたオフセット方向」と「対象を追って回転する向き」
//   がズレていき、ロックした瞬間だけ整列していたプレイヤーが画角から
//   外れてしまう不具合が発生した。
// 3段階目(現在): 位置の基準そのものを「プレイヤー→対象の水平方向」から
//   毎フレーム再計算する(UpdateLockedPosition()参照)。カメラ・
//   プレイヤー・対象が一直線に並ばないよう、対象方向からlockOnYawBias_
//   だけ左右にずらした「肩越し」の位置に構える。これによりプレイヤーが
//   動いても位置と向きが常に対象との関係で決まるため、ズレが蓄積しない。
//   向き自体はUpdateLockedPositionで決まった位置からTryLookAtLockedTarget()
//   で改めて対象へ向ける。
class CameraFollowComponent : public ComponentBase {
public:
	explicit CameraFollowComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// 存在する場合のみマウス軌道回転を優先して使う
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();
	}

	// 別オブジェクトのためHandleで受け取る
	void SetTarget(Handle<CameraTargetComponent> target) { target_ = target; }
	void SetLocalOffset(const Math::Vector3& offset) { localOffset_ = offset; }

	// 追従対象の向きにもカメラを合わせたい場合はtrue(三人称カメラ等)。
	void SetFollowRotation(bool follow) { followRotation_ = follow; }

	// ロック中、ロック対象への注視方向へ向き直る速さ
	void SetLockOnTurnSpeed(float speed) { lockOnTurnSpeed_ = speed; }

	// ロック中の見上げ/見下ろしの限界
	void SetLockOnPitchLimits(float minPitch, float maxPitch) {
		lockPitchMin_ = minPitch;
		lockPitchMax_ = maxPitch;
	}

	// ロック中の位置の調整用
	void SetLockOnShoulderOffset(float yawBias, float positionPitch) {
		lockOnYawBias_ = yawBias;
		lockOnPositionPitch_ = positionPitch;
	}

	void Resolve(float deltaTime) {
		if (transform_ == nullptr) return;

		CameraTargetComponent* target = target_.Resolve();
		if (target == nullptr) return;

		const Math::Vector3 playerPos = target->GetTargetPosition();

		GameObject* lockedTarget = nullptr;
		if (const SceneContext* context = GetOwner()->GetContext()) {
			lockedTarget = context->lockedTarget.Resolve();
		}

		const bool positioned = (lockedTarget != nullptr) && UpdateLockedPosition(playerPos, lockedTarget);

		if (!positioned) {
			wasLockedLastFrame_ = false;

			const Math::Quaternion offsetRotation =
				(orbit_ != nullptr) ? orbit_->GetOrbitRotation() : target->GetTargetRotation();

			// ローカルオフセットを回転基準で回転させ、ワールド空間のオフセットにする。
			const Math::Vector3 worldOffset =
				Math::Vector3::Transform(localOffset_, offsetRotation);

			transform_->SetPosition(playerPos + worldOffset);

			if (followRotation_) {
				transform_->SetRotation(offsetRotation);
			}
			return;
		}

		// ここに来た時点で位置(transform_の座標)はロック基準で確定済み。
		// followRotation_がtrueの場合のみ、その位置から対象への向きを
		// 改めて計算する(向きを一切追従させたくない固定角カメラの場合は
		// 位置だけロックに追従させ、向きには触れない)。
		if (followRotation_ && !TryLookAtLockedTarget(lockedTarget, deltaTime)) {
			wasLockedLastFrame_ = false;
			orbit_->SetYawPitch(lockYaw_, lockPitch_);
		}
	}

private:
	// プレイヤー→対象の水平方向を基準にロック中のカメラ位置を計算
	bool UpdateLockedPosition(const Math::Vector3& playerPos, GameObject* lockedTarget) {
		TransformComponent* targetTransform = lockedTarget->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return false;

		Math::Vector3 toTarget = targetTransform->GetPosition() - playerPos;
		toTarget.y = 0.0f; 
		const float horizontalLenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
		if (horizontalLenSq <= kMinDirectionLengthSq) return false;

		const float aimYaw = std::atan2(toTarget.x, toTarget.z);

		const Math::Quaternion positionRotation =
			Math::Quaternion::CreateFromYawPitchRoll(aimYaw + lockOnYawBias_, lockOnPositionPitch_, 0.0f);

		const Math::Vector3 worldOffset = Math::Vector3::Transform(localOffset_, positionRotation);
		transform_->SetPosition(playerPos + worldOffset);
		return true;
	}

	// 現在のカメラ位置からlockedTargetを見る回転を計算
	bool TryLookAtLockedTarget(GameObject* target, float deltaTime) {
		TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return false;

		const Math::Vector3 dir = targetTransform->GetPosition() - transform_->GetPosition();
		const float horizontalLenSq = dir.x * dir.x + dir.z * dir.z;
		if (horizontalLenSq <= kMinDirectionLengthSq) return false;

		const float horizontalLen = std::sqrt(horizontalLenSq);
		const float desiredYaw = std::atan2(dir.x, dir.z);
		const float desiredPitch = std::atan2(-dir.y, horizontalLen);

		if (!wasLockedLastFrame_) {
			const float maxDelta = lockOnTurnSpeed_ * deltaTime;
			lockYaw_ = ApproachAngle(lockYaw_, desiredYaw, maxDelta);
			lockPitch_ = std::clamp(ApproachAngle(lockPitch_, desiredPitch, maxDelta), lockPitchMin_, lockPitchMax_);
		}
		else {
			const float maxDelta = lockOnTurnSpeed_ * deltaTime;
			lockYaw_ = ApproachAngle(lockYaw_, desiredYaw, maxDelta);
			lockPitch_ = std::clamp(ApproachAngle(lockPitch_, desiredPitch, maxDelta), lockPitchMin_, lockPitchMax_);
		}
		wasLockedLastFrame_ = true;

		transform_->SetRotation(Math::Quaternion::CreateFromYawPitchRoll(lockYaw_, lockPitch_, 0.0f));
		return true;
	}

	// 角度(ラジアン)をcurrentからtargetへ、1フレームあたり最大maxDeltaだけ近づける
	static float ApproachAngle(float current, float target, float maxDelta) {
		const float diff = std::atan2(std::sin(target - current), std::cos(target - current));
		if (diff > maxDelta) return current + maxDelta;
		if (diff < -maxDelta) return current - maxDelta;
		return current + diff;
	}

	TransformComponent* transform_ = nullptr;
	CameraOrbitComponent* orbit_ = nullptr;        // 同一GameObjectの兄弟コンポーネントなので生ポインタのまま
	Handle<CameraTargetComponent> target_;         // 別GameObjectの参照なのでHandle化
	Math::Vector3 localOffset_{ 0.0f, 0.0f, -10.0f };
	bool followRotation_ = true;

	// ロック中の向き
	float lockYaw_ = 0.0f;
	float lockPitch_ = 0.0f;
	bool wasLockedLastFrame_ = false; // ロックし始めた瞬間かどうかの判定に使う

	// ロック中にyaw/pitchを対象方向へ近づける速さ
	float lockOnTurnSpeed_ = 6.0f;

	// ロック中の見上げ/見下ろしの限界
	float lockPitchMin_ = -1.2f;
	float lockPitchMax_ = 1.2f;

	// ロック中どれだけ左右にずらして構えるか
	float lockOnYawBias_ = 0.5f;
	// ロック中の位置の見下ろし角度
	float lockOnPositionPitch_ = 0.35f;

	// ロック対象方向の水平成分がこれ以下の場合は向きを更新しない閾値
	static constexpr float kMinDirectionLengthSq = 1e-6f;
};