#pragma once
#include "../Camera/CameraTargetComponent.h"
#include "CameraOrbitComponent.h"
#include "../../Tags/ICameraTarget.h"

// カメラの追従ロジック
class CameraFollowComponent : public ComponentBase {
public:
	explicit CameraFollowComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// 存在する場合のみマウス軌道回転を優先して使う
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();
	}

	// 別オブジェクトのためHandleで受け取る
	void SetTarget(Handle<CameraTargetComponent> target) { target_ = target; }
	void SetDistance(const float& distance) { cameraDistance_ = distance; }

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

		const Math::Vector3 playerPos = target->GetFixationPoint();

		// オービット操作の影響を受けない、ターゲット自体の移動だけをイージングする基準点
		if (!anchorInitialized_) {
			easedAnchorPosition_ = playerPos;
			anchorInitialized_ = true;
		}
		else {
			easedAnchorPosition_ = EaseTowards(easedAnchorPosition_, playerPos, deltaTime);
		}

		GameObject* lockedTarget = nullptr;
		if (const SceneContext* context = GetOwner()->GetContext()) {
			lockedTarget = context->lockedTarget.Resolve();
		}

		Math::Vector3 lockedPosition;
		const bool positioned = (lockedTarget != nullptr) && ComputeLockedPosition(easedAnchorPosition_, lockedTarget, lockedPosition);
		const bool isTransitioning = (positioned != wasPositionedLastFrame_);

		// --- ロック状態の切り替わり時の向きの同期 ---
		if (isTransitioning) {
			if (positioned) {
				// ロック開始の瞬間、lockYaw_/lockPitch_を「今実際にカメラが向いている方向」で初期化しておく
				if (orbit_ != nullptr) {
					lockYaw_ = orbit_->GetYaw();
					lockPitch_ = std::clamp(orbit_->GetPitch(), lockPitchMin_, lockPitchMax_);
				}
				else if (transform_ != nullptr) {
					const Math::Vector3 forward = transform_->GetForward();
					const float horizontalLen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
					lockYaw_ = std::atan2(forward.x, forward.z);
					lockPitch_ = std::clamp(std::atan2(-forward.y, horizontalLen), lockPitchMin_, lockPitchMax_);
				}
			}
			else if (orbit_ != nullptr) {
				// ロック解除の瞬間は逆に、orbit_側をロック中の向きに合わせておく
				orbit_->SetYawPitch(lockYaw_, lockPitch_);
			}
		}

		// 通常追従(非ロック時)の目標位置。上の同期が終わった後に計算する
		// ことで、切り替わったフレームでも正しい基準(同期済みのorbit_)を使う。
		const Math::Quaternion offsetRotation =
			(orbit_ != nullptr) ? orbit_->GetOrbitRotation() : target->GetTargetRotation();
		const Math::Vector3 followWorldOffset = Math::Vector3::Transform(Math::Vector3::Forward, offsetRotation) * cameraDistance_;
		const Math::Vector3 followPosition = easedAnchorPosition_ + followWorldOffset;

		// --- ロック状態の切り替わりを検出し、位置の飛びをイージングで吸収する ---
		// ロックON/OFFの瞬間は目標位置の計算式そのものが変わるため、何もしないと
		// カメラが瞬間移動してしまう。切り替わったフレームで「今の実位置」と
		// 「切り替わった後の新しい目標位置」との差分(=横にずれていた量など)を
		// positionEaseOffset_として保持しておき、以後は目標位置にこの差分を
		// 足しながら時間経過で0へ減衰させることで、自然な遷移にする。
		if (isTransitioning) {
			const Math::Vector3 newTargetPosition = positioned ? lockedPosition : followPosition;
			positionEaseOffset_ = transform_->GetPosition() - newTargetPosition;
			positionEaseTimer_ = 0.0f;
			wasPositionedLastFrame_ = positioned;
		}

		const Math::Vector3 easeOffset = ConsumePositionEaseOffset(deltaTime);

		if (!positioned) {
			wasLockedLastFrame_ = false;

			// 基準点(easedAnchorPosition_)側で既にイージング済みのため、ここでは直接反映する
			transform_->SetPosition(followPosition + easeOffset);

			if (followRotation_) {
				transform_->SetRotation(offsetRotation);
			}
			return;
		}

		// 基準点(easedAnchorPosition_)側で既にイージング済みのため、ここでは直接反映する
		transform_->SetPosition(lockedPosition + easeOffset);

		// followRotation_がtrueの場合のみ、その位置から対象への向きを
		// 改めて計算する(向きを一切追従させたくない固定角カメラの場合は
		// 位置だけロックに追従させ、向きには触れない)。
		if (followRotation_ && !TryLookAtLockedTarget(lockedTarget, deltaTime)) {
			wasLockedLastFrame_ = false;
			orbit_->SetYawPitch(lockYaw_, lockPitch_);
		}
	}

	// ロック状態切り替え時のイージングにかける秒数
	void SetPositionEaseDuration(float duration) { positionEaseDuration_ = duration; }

	// ターゲット自体の水平方向(XZ)移動に対する追従イージング速度(オービット操作には影響しない)
	void SetFollowEaseSpeed(float speed) { followEaseSpeed_ = speed; }

	// ターゲット自体の垂直方向(Y)移動に対する追従イージング速度。デフォルト0=イージングなしで即座に追従(浮遊感対策)
	void SetFollowEaseSpeedVertical(float speed) { followEaseSpeedVertical_ = speed; }

private:
	// プレイヤー→対象の水平方向を基準にロック中のカメラ位置を計算する
	bool ComputeLockedPosition(const Math::Vector3& playerPos, GameObject* lockedTarget, Math::Vector3& outPosition) const {
		TransformComponent* targetTransform = lockedTarget->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return false;

		Math::Vector3 toTarget = targetTransform->GetPosition() - playerPos;
		toTarget.y = 0.0f;
		const float horizontalLenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
		if (horizontalLenSq <= kMinDirectionLengthSq) return false;

		const float aimYaw = std::atan2(toTarget.x, toTarget.z);

		const Math::Quaternion positionRotation =
			Math::Quaternion::CreateFromYawPitchRoll(aimYaw + lockOnYawBias_, lockOnPositionPitch_, 0.0f);

		const Math::Vector3 worldOffset = Math::Vector3::Transform(Math::Vector3::Forward, positionRotation) * cameraDistance_;
		outPosition = playerPos + worldOffset;
		return true;
	}

	// positionEaseOffset_を経過時間に応じて0へ減衰させ、その時点の値を返す。
	// 呼び出すたびにタイマーを進める(1回のResolve()につき1回呼ぶ想定)。
	// 減衰カーブはEaseOutCubic(最初は速く減り、終端でなめらかに0へ収束する)。
	Math::Vector3 ConsumePositionEaseOffset(float deltaTime) {
		if (positionEaseTimer_ >= positionEaseDuration_ || positionEaseOffset_.LengthSquared() <= kEaseOffsetEpsilonSq) {
			return Math::Vector3::Zero;
		}

		positionEaseTimer_ = std::min(positionEaseTimer_ + deltaTime, positionEaseDuration_);
		const float t = (positionEaseDuration_ > 0.0f) ? (positionEaseTimer_ / positionEaseDuration_) : 1.0f;

		const float remainingRatio = std::pow(1.0f - t, 3.0f);
		return positionEaseOffset_ * remainingRatio;
	}

	// currentからdesiredへ、水平(XZ)と垂直(Y)を別速度でイージング接近させる(垂直を分けるのは浮遊感対策)。
	// 追従基準点(easedAnchorPosition_)の平滑化に使う
	Math::Vector3 EaseTowards(const Math::Vector3& current, const Math::Vector3& desired, float deltaTime) const {
		const float horizontalT = (followEaseSpeed_ > 0.0f) ? (1.0f - std::exp(-followEaseSpeed_ * deltaTime)) : 1.0f;
		const float verticalT = (followEaseSpeedVertical_ > 0.0f) ? (1.0f - std::exp(-followEaseSpeedVertical_ * deltaTime)) : 1.0f;

		return Math::Vector3(
			current.x + (desired.x - current.x) * horizontalT,
			current.y + (desired.y - current.y) * verticalT,
			current.z + (desired.z - current.z) * horizontalT
		);
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
	float cameraDistance_ = 5.f;
	bool followRotation_ = true;
	// 水平(XZ)方向の追従イージング速度。0以下で従来通り瞬間追従
	float followEaseSpeed_ = 8.0f;
	// 垂直(Y)方向の追従イージング速度。デフォルト0=イージングなしで即座に追従(浮遊感対策)
	float followEaseSpeedVertical_ = 0.0f;

	// オービット操作の影響を受けない、ターゲット自体の移動をイージングした基準点
	Math::Vector3 easedAnchorPosition_ = Math::Vector3::Zero;
	bool anchorInitialized_ = false;

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

	// --- ロック状態切り替え時の位置イージング -----------------------
	// ロックON/OFFが切り替わった瞬間の「実位置 - 新しい目標位置」を保持し、
	// 以後のフレームで目標位置に足しながら0へ減衰させる差分ベクトル。
	Math::Vector3 positionEaseOffset_ = Math::Vector3::Zero;
	float positionEaseTimer_ = 0.0f;
	// イージングにかける秒数。0にすると従来通り瞬間切り替えになる。
	float positionEaseDuration_ = 0.35f;
	// 前フレームでComputeLockedPositionが成功していた(=ロック位置を使っていた)か。
	// wasLockedLastFrame_(向きの角度補間用)とは別管理。
	bool wasPositionedLastFrame_ = false;

	// イージングオフセットがこれ以下になったら完了とみなす閾値
	static constexpr float kEaseOffsetEpsilonSq = 1e-6f;
};