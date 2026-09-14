#pragma once
#include "../Camera/CameraTargetComponent.h"
#include "CameraOrbitComponent.h"
#include "../../Tags/ICameraTarget.h"

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
//   毎フレーム再計算する(ComputeLockedPosition()参照)。カメラ・
//   プレイヤー・対象が一直線に並ばないよう、対象方向からlockOnYawBias_
//   だけ左右にずらした「肩越し」の位置に構える。これによりプレイヤーが
//   動いても位置と向きが常に対象との関係で決まるため、ズレが蓄積しない。
//   向き自体はComputeLockedPositionで決まった位置からTryLookAtLockedTarget()
//   で改めて対象へ向ける。
//
// 4段階目(今回追加): ロックON/OFFの瞬間は目標位置の計算式そのものが
//   切り替わるため、何もしないとカメラが瞬間移動してしまう。切り替わった
//   フレームで「実位置と新しい目標位置の差分」をpositionEaseOffset_として
//   保持し、以後は目標位置にこの差分を足しながら時間経過で0へ減衰させる
//   ことで、ロック開始/解除どちらの向きの遷移も自然なイージングにしている
//   (Resolve()内、ComputeLockedPosition呼び出し直後のブロック参照)。
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

		Math::Vector3 lockedPosition;
		const bool positioned = (lockedTarget != nullptr) && ComputeLockedPosition(playerPos, lockedTarget, lockedPosition);
		const bool isTransitioning = (positioned != wasPositionedLastFrame_);

		// --- ロック状態の切り替わり時の向きの同期 ---
		// 【重要】これはこの下のoffsetRotation/followPosition計算より
		// 必ず先に行うこと。もし後回しにすると、切り替わったまさにこの
		// フレームで、まだ同期前の(切り替わる前の)古いorbit_の値を使って
		// offsetRotationやfollowPositionを計算してしまい、次のフレームで
		// ようやく正しい値に切り替わる…という1フレームだけのズレが生じる。
		// これが解除した瞬間に画面が「ガクッ」となる直接の原因だった。
		if (isTransitioning) {
			if (positioned) {
				// ロック開始の瞬間、lockYaw_/lockPitch_を「今実際にカメラが
				// 向いている方向」で初期化しておく。これをしないと、
				// 最初にロックした時(デフォルト値0,0のまま)や、一度解除して
				// 向きを変えてから再ロックした時(前回ロック時の値が残ったまま)に、
				// TryLookAtLockedTarget()内のApproachAngleが実際のカメラの
				// 向きとは無関係な値から対象方向への補間を始めてしまい、
				// 位置はイージングしていても向きだけ急に振れて不自然に見える。
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
				// ロック解除の瞬間は逆に、orbit_側をロック中の向きに合わせておく。
				// そうしないと、この直後に向きの基準がlockYaw_/lockPitch_から
				// orbit_ベースへ切り替わった際、向きだけ瞬間的に飛んでしまう
				// (位置はイージングするのに向きだけ飛ぶと不自然に見えるため)。
				orbit_->SetYawPitch(lockYaw_, lockPitch_);
			}
		}

		// 通常追従(非ロック時)の目標位置。上の同期が終わった後に計算する
		// ことで、切り替わったフレームでも正しい基準(同期済みのorbit_)を使う。
		const Math::Quaternion offsetRotation =
			(orbit_ != nullptr) ? orbit_->GetOrbitRotation() : target->GetTargetRotation();
		const Math::Vector3 followWorldOffset = Math::Vector3::Transform(localOffset_, offsetRotation);
		const Math::Vector3 followPosition = playerPos + followWorldOffset;

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

			transform_->SetPosition(followPosition + easeOffset);

			if (followRotation_) {
				transform_->SetRotation(offsetRotation);
			}
			return;
		}

		// ここでの位置(transform_の座標)は、ロック基準の目標位置に
		// イージング分のオフセットを足したもので確定させる。
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

private:
	// プレイヤー→対象の水平方向を基準にロック中のカメラ位置を計算する。
	// 【注意】以前はここでtransform_->SetPositionまで行っていたが、
	// イージング用オフセットを足し込んでから最終的な位置を確定させたいため、
	// 計算結果をoutPositionへ書き込むだけにして、実際の反映はResolve()側で行う。
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

		const Math::Vector3 worldOffset = Math::Vector3::Transform(localOffset_, positionRotation);
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