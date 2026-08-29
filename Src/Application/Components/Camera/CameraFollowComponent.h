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
// 「位置」はこれまで通りorbit_基準(マウス操作によるshoulder位置)のまま
// 変えず、「向き」だけを対象への注視方向に差し替える。
//
// 以前はCameraOrbitComponent側でロック中のyaw/pitchを対象方向へ直接
// 書き換えていたが、その値は位置(このクラスのオフセット計算)にも
// 使われているため、位置までロック対象方向へ引っ張られてしまい、
// カメラ・プレイヤー・ロック対象が一直線に並んでプレイヤー自身が
// 対象を隠してしまう不具合があった。位置と向きの計算を分離すること
// でこれを解消する。
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

	// Update(移動・入力解決)が全て終わった後に追従先を読みたいため、
	// PostUpdateで計算する。
	void PostUpdate(float deltaTime) override {
		if (transform_ == nullptr) return;

		// 毎フレームResolve()で有効性を確認する。対象がすでに破棄されて
		// いれば単にnullptrが返るだけで、以前のようにダングリング
		// ポインタを触ってUBになることがない。
		CameraTargetComponent* target = target_.Resolve();
		if (target == nullptr) return;

		// CameraOrbitComponentがあればマウス軌道回転を、無ければ従来通り
		// 対象自身の向きをオフセットの回転基準にする。
		// 【注意】ここは「位置」の計算専用。ロック中でもこの回転基準は
		// 変えない(位置までロック対象方向へ引っ張らないための分離。
		// クラス冒頭コメント参照)。
		const Math::Quaternion offsetRotation =
			(orbit_ != nullptr) ? orbit_->GetOrbitRotation() : target->GetTargetRotation();

		const Math::Vector3 targetPos = target->GetTargetPosition();

		// ローカルオフセットを回転基準で回転させ、ワールド空間のオフセットにする。
		const Math::Vector3 worldOffset =
			Math::Vector3::Transform(localOffset_, offsetRotation);

		transform_->SetPosition(targetPos + worldOffset);

		// ロック中は「向き」だけを対象への注視方向に差し替える
		// (位置は上ですでに確定済みで変更しない)。followRotation_がfalse
		// (向きを一切追従させない固定角カメラ)の場合はロック中も対象外とする。
		if (followRotation_) {
			if (const SceneContext* context = GetOwner()->GetContext()) {
				if (GameObject* lockedTarget = context->lockedTarget.Resolve()) {
					if (TryLookAtLockedTarget(lockedTarget, deltaTime)) {
						return;
					}
				}
			}
			transform_->SetRotation(offsetRotation);
		}
	}

private:
	// 現在のカメラ位置からlockedTargetを見る回転を計算し、transform_の
	// 回転として設定する。位置はPostUpdate()側で既に確定済みのものを使う
	// (ここでは位置に一切触れない)。対象のTransformComponentが無い、
	// あるいは対象がほぼ真上/真下(水平成分がほぼ0)の場合はfalseを返し、
	// 呼び出し側の通常のfollowRotation_処理にフォールバックさせる。
	bool TryLookAtLockedTarget(GameObject* target, float deltaTime) {
		TransformComponent* targetTransform = target->GetComponent<TransformComponent>();
		if (targetTransform == nullptr) return false;

		const Math::Vector3 dir = targetTransform->GetPosition() - transform_->GetPosition();
		const float horizontalLenSq = dir.x * dir.x + dir.z * dir.z;
		if (horizontalLenSq <= kMinDirectionLengthSq) return false;

		// 【要確認】+Z前方の左手系を想定したyaw/pitch算出
		// (PlayerStatusController::FaceTowards/ClassifyEvadeDirectionと
		//  同じ座標系前提の確認が必要な箇所。実機で左右/上下が逆に見える
		//  場合はここの符号を調整すること)。
		const float horizontalLen = std::sqrt(horizontalLenSq);
		const float desiredYaw = std::atan2(dir.x, dir.z);
		const float desiredPitch = std::atan2(dir.y, horizontalLen);

		// 現在の向き(前フレームまでにこの関数が設定した回転)からyaw/pitchを
		// 逆算し、瞬時に向き直さず一定速度で対象方向へ近づける。
		float currentYaw = 0.0f;
		float currentPitch = 0.0f;
		DecomposeYawPitch(transform_->GetForward(), currentYaw, currentPitch);

		const float maxDelta = lockOnTurnSpeed_ * deltaTime;
		const float yaw = ApproachAngle(currentYaw, desiredYaw, maxDelta);
		const float pitch = ApproachAngle(currentPitch, desiredPitch, maxDelta);

		transform_->SetRotation(Math::Quaternion::CreateFromYawPitchRoll(yaw, pitch, 0.0f));
		return true;
	}

	// 前方ベクトルからyaw/pitchを逆算する(TryLookAtLockedTargetの
	// yaw/pitch算出と対になる符号規約。要確認箇所も同じ)。
	static void DecomposeYawPitch(const Math::Vector3& forward, float& outYaw, float& outPitch) {
		const float horizontalLen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
		outYaw = std::atan2(forward.x, forward.z);
		outPitch = std::atan2(forward.y, horizontalLen);
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

	// ロック中にyaw/pitchを対象方向へ近づける速さ(ラジアン/秒、仮の値)。
	float lockOnTurnSpeed_ = 6.0f;

	// ロック対象方向の水平成分がこれ以下(ほぼ真上/真下、または自分と同じ
	// 位置)の場合は向きを更新しない、という閾値。
	static constexpr float kMinDirectionLengthSq = 1e-6f;
};