#pragma once
#include <cmath>
#include "../Tags/ICameraTarget.h"
#include "../Transform/TransformComponent.h"
#include "../Camera/CameraTargetComponent.h"
#include "../../Core/Handle.h"
#include "../../Core/SceneContext.h"
#include "CameraOrbitComponent.h"

// ============================================================
// カメラの追従"ロジック"だけを持つコンポーネント。
//
// CameraComponent(「これがカメラである」ことを表すだけ)とは完全に
// 独立しており、お互いの存在を知らない。両者は同じGameObjectの
// TransformComponentを介してのみやり取りする。
//
// 追従対象(CameraTargetComponent)は別GameObject上のコンポーネントであり、
// 生存期間がこのコンポーネントと結びついていない(対象が死亡演出で
// Destroyされることがある)ため、生ポインタではなくHandle<T>で保持する
// (Handle.hの使い分けルール: 生存期間が結びついていない相手を
// フレームをまたいで指し続ける場合はHandle、が該当する)。
// 以前はICameraTarget*で持っていたが、これは「同一GameObject内の
// 兄弟コンポーネント」向けの生ポインタと区別がつかず、対象破棄後に
// ダングリングポインタを踏む欠陥があった。
//
// CameraOrbitComponent(マウスによる軌道回転)は同一GameObject上の
// 兄弟コンポーネントなので、こちらは従来通り生ポインタのままでよい。
//
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
// ============================================================
class CameraFollowComponent : public ComponentBase {
public:
	explicit CameraFollowComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// 無くてもよい(任意)。存在する場合のみマウス軌道回転を優先して使う。
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();
	}

	// 別GameObjectのコンポーネントを参照するため、生ポインタではなく
	// Handle<T>で受け取る(呼び出し側で
	// Handle<CameraTargetComponent>(targetObject->GetComponent<CameraTargetComponent>())
	// のように構築して渡す)。
	void SetTarget(Handle<CameraTargetComponent> target) { target_ = target; }
	void SetLocalOffset(const Math::Vector3& offset) { localOffset_ = offset; }

	// 追従対象の向きにもカメラを合わせたい場合はtrue(三人称カメラ等)。
	void SetFollowRotation(bool follow) { followRotation_ = follow; }

	// ロック中、ロック対象への注視方向へ向き直る速さ(ラジアン/秒)。
	// 値が大きいほど瞬時に近い向き直りになる。
	void SetLockOnTurnSpeed(float speed) { lockOnTurnSpeed_ = speed; }

	// ロック中の見上げ/見下ろしの限界(ラジアン)。CameraOrbitComponent::
	// SetPitchLimits()と同じ役割だが、こちらは向き専用の値を独立に持つ。
	void SetLockOnPitchLimits(float minPitch, float maxPitch) {
		lockPitchMin_ = minPitch;
		lockPitchMax_ = maxPitch;
	}

	// ロック中の位置(UpdateLockedPosition参照)の調整用。
	// yawBiasは「プレイヤー→対象」の方向から左右にどれだけずらして
	// 構えるか(ラジアン。0だと一直線に並んでプレイヤーが対象を隠す
	// 不具合が再発する)。positionPitchはその位置の高さ(見下ろし角度)。
	void SetLockOnShoulderOffset(float yawBias, float positionPitch) {
		lockOnYawBias_ = yawBias;
		lockOnPositionPitch_ = positionPitch;
	}

	// Update(移動・入力解決)が全て終わった後に追従先を読みたいため、
	// PostUpdateで計算する。
	void PostUpdate(float deltaTime) override {
		if (transform_ == nullptr) return;

		// 毎フレームResolve()で有効性を確認する。対象がすでに破棄されて
		// いれば単にnullptrが返るだけで、以前のようにダングリング
		// ポインタを触ってUBになることがない。
		CameraTargetComponent* target = target_.Resolve();
		if (target == nullptr) return;

		const Math::Vector3 playerPos = target->GetTargetPosition();

		GameObject* lockedTarget = nullptr;
		if (const SceneContext* context = GetOwner()->GetContext()) {
			lockedTarget = context->lockedTarget.Resolve();
		}

		// ロック中は位置もプレイヤー→対象の関係から毎フレーム計算し直す
		// (クラス冒頭コメントの3段階目参照)。
		const bool positioned = (lockedTarget != nullptr) && UpdateLockedPosition(playerPos, lockedTarget);

		if (!positioned) {
			// ロックしていない、またはロック中でも位置を決められなかった
			// (対象のTransformComponentが無い、対象が真上/真下等)場合は、
			// 従来通りorbit_基準の位置・向きに戻る。
			wasLockedLastFrame_ = false;

			// CameraOrbitComponentがあればマウス軌道回転を、無ければ従来通り
			// 対象自身の向きをオフセットの回転基準にする。
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
		}
	}

private:
	// プレイヤー→対象の水平方向を基準に、ロック中のカメラ位置(肩越しの
	// 位置)を計算してtransform_へ設定する。対象のTransformComponentが
	// 無い、あるいは対象がプレイヤーのほぼ真上/真下(水平成分がほぼ0)の
	// 場合はfalseを返し、呼び出し側の通常のorbit_基準の位置にフォール
	// バックさせる。
	//
	// カメラ・プレイヤー・対象が一直線に並ぶとプレイヤーが対象を隠して
	// しまう(以前の不具合)ため、対象方向からlockOnYawBias_だけ左右に
	// ずらした位置に構える。プレイヤーの位置・対象の位置いずれが動いても
	// 毎フレームこの関数で計算し直すため、位置と向きの基準がズレて
	// いくことがない(以前の不具合の解消。クラス冒頭コメント参照)。
	bool UpdateLockedPosition(const Math::Vector3& playerPos, GameObject* lockedTarget) {
		TransformComponent* targetTransform = lockedTarget->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return false;

		Math::Vector3 toTarget = targetTransform->GetPosition() - playerPos;
		toTarget.y = 0.0f; // 位置決めの基準は水平方向のみで十分(高さはlockOnPositionPitch_側で扱う)
		const float horizontalLenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
		if (horizontalLenSq <= kMinDirectionLengthSq) return false;

		// 【要確認】+Z前方の左手系を想定したyaw算出
		// (PlayerStatusController::FaceTowards/ClassifyEvadeDirectionと
		//  同じ座標系前提の確認が必要な箇所。実機で左右が逆に見える場合は
		//  ここの符号を調整すること)。
		const float aimYaw = std::atan2(toTarget.x, toTarget.z);

		const Math::Quaternion positionRotation =
			Math::Quaternion::CreateFromYawPitchRoll(aimYaw + lockOnYawBias_, lockOnPositionPitch_, 0.0f);

		const Math::Vector3 worldOffset = Math::Vector3::Transform(localOffset_, positionRotation);
		transform_->SetPosition(playerPos + worldOffset);
		return true;
	}

	// 現在のカメラ位置からlockedTargetを見る回転を計算し、transform_の
	// 回転として設定する。位置はUpdateLockedPosition()側で既に確定済みの
	// ものを使う(ここでは位置に一切触れない)。対象のTransformComponentが
	// 無い、あるいは対象がほぼ真上/真下(水平成分がほぼ0)の場合はfalseを
	// 返す。
	bool TryLookAtLockedTarget(GameObject* target, float deltaTime) {
		TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return false;

		const Math::Vector3 dir = targetTransform->GetPosition() - transform_->GetPosition();
		const float horizontalLenSq = dir.x * dir.x + dir.z * dir.z;
		if (horizontalLenSq <= kMinDirectionLengthSq) return false;

		// 【要確認/修正済み】+Z前方の左手系を想定したyaw/pitch算出。
		// pitchは当初dir.yをそのまま使っていたが、実機で「接近するほど
		// カメラが上を向いて対象を見失う」不具合が確認されたため、符号を
		// 反転している(dir.yが負=対象が下にある時、下を向く向きになる
		// よう修正)。もし逆に見える場合はここの符号を再度調整すること。
		const float horizontalLen = std::sqrt(horizontalLenSq);
		const float desiredYaw = std::atan2(dir.x, dir.z);
		const float desiredPitch = std::atan2(-dir.y, horizontalLen);

		// 【重要】現在の向きをtransform_->GetForward()から逆算すること
		// (デコード)はしない。lockYaw_/lockPitch_というこのクラス自身の
		// 状態として角度を保持し続け、Quaternionの中身を読み返さないことで、
		// エンコード/デコードの往復不一致による振動を根本的に防ぐ
		// (クラス冒頭コメント参照)。
		if (!wasLockedLastFrame_) {
			// ロックし始めた瞬間は、いきなり対象方向へスナップしてよい
			// (滑らかに近づける基準となる「前回の向き」がまだ無いため)。
			lockYaw_ = desiredYaw;
			lockPitch_ = desiredPitch;
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
	CameraOrbitComponent* orbit_ = nullptr;        // 同一GameObjectの兄弟コンポーネントなので生ポインタのまま
	Handle<CameraTargetComponent> target_;         // 別GameObjectの参照なのでHandle化
	Math::Vector3 localOffset_{ 0.0f, 0.0f, -10.0f };
	bool followRotation_ = true;

	// ロック中の向き(yaw/pitch)をこのクラス自身が保持する状態。
	// Quaternionから逆算しない理由はクラス冒頭コメント参照。
	float lockYaw_ = 0.0f;
	float lockPitch_ = 0.0f;
	bool wasLockedLastFrame_ = false; // ロックし始めた瞬間かどうかの判定に使う

	// ロック中にyaw/pitchを対象方向へ近づける速さ(ラジアン/秒、仮の値)。
	float lockOnTurnSpeed_ = 6.0f;

	// ロック中の見上げ/見下ろしの限界(ラジアン、仮の値)。
	float lockPitchMin_ = -1.2f;
	float lockPitchMax_ = 1.2f;

	// ロック中の位置(UpdateLockedPosition参照)。「プレイヤー→対象」の
	// 方向からどれだけ左右にずらして構えるか(ラジアン、仮の値。
	// 約28.6度)。0にすると一直線に並ぶ不具合が再発するので0にしないこと。
	float lockOnYawBias_ = 0.5f;
	// ロック中の位置の見下ろし角度(ラジアン、仮の値。約20度)。
	// 【要確認】符号がプロジェクトのpitch規約次第で、カメラが低い/
	// 地面に潜る位置になる場合は符号を反転すること。
	float lockOnPositionPitch_ = 0.35f;

	// ロック対象方向の水平成分がこれ以下(ほぼ真上/真下、または自分と同じ
	// 位置)の場合は向きを更新しない、という閾値。
	static constexpr float kMinDirectionLengthSq = 1e-6f;
};