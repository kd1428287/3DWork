#pragma once
#include "CameraFollowComponent.h"
#include "CameraCollisionComponent.h"
#include "CameraShakeComponent.h"
#include "CameraOrbitComponent.h"

// カメラを表すコンポーネント
// 責務は以下の4つ:
//   1. 「これがカメラである」ことの管理(SceneContext::activeCameraへの登録)
//   2. 外部への正対ベクトル/座標等のアクセサ提供
//   3. 同一GameObject上のカメラ関連コンポーネント(Follow/Collision/Shake)の実行順序の保証
//   4. プレイヤーの移動入力をカメラ基準に変換するための基準回転の提供
//      (GetMovementYawRotation()。旧InputSystemが持っていた変換ロジックをここに集約)
class CameraComponent : public ComponentBase {
public:
	explicit CameraComponent(GameObject* owner) : ComponentBase(owner) 
	{
		if (!camera_)
		{
			camera_ = std::make_unique<KdCamera>();
		}
		camera_->SetProjectionMatrix(60);
	}

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		follow_ = GetOwner()->GetComponent<CameraFollowComponent>();
		collision_ = GetOwner()->GetComponent<CameraCollisionComponent>();
		shake_ = GetOwner()->GetComponent<CameraShakeComponent>();

		// 移動方向のフォールバック基準としてのみ使う(無くてもよい)。
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();

		if (auto* ctx = GetOwner()->GetContext()) {
			ctx->activeCamera = this;  // 自分をアクティブカメラとして登録
		}
	}

	void OnDestroy() override {
		if (auto* ctx = GetOwner()->GetContext()) {
			if (ctx->activeCamera == this) {
				ctx->activeCamera = nullptr;
			}
		}
	}

	// Follow→Collision→Shakeの順で解決する
	void PostUpdate(float deltaTime) override
	{
		if (follow_)    follow_->Resolve(deltaTime);
		if (collision_) collision_->Resolve(deltaTime);

		if (shake_ && transform_) {
			const SceneContext* ctx = GetOwner()->GetContext();
			const float shakeDt = ctx ? ctx->unscaledDeltaTime : deltaTime;

			shake_->Resolve(shakeDt);
			transform_->SetPosition(transform_->GetPosition() + shake_->GetPositionOffset());
			transform_->SetRotation(transform_->GetRotation() * shake_->GetRotationOffset());
		}
	}

	Math::Vector3 GetPosition() const { return transform_ ? transform_->GetPosition() : Math::Vector3{}; }

	// カメラの向いている方向(正規化済み)。
	// 【注意】PlayerLockOnComponent::FindNearestToScreenCenter()など、
	// 実際の視線方向をそのまま必要とする用途専用。移動方向の算出には
	// 下のGetMovementYawRotation()を使うこと(こちらに真上/真下フォールバックを
	// 混ぜ込むと、ロックオン対象選択の判定基準まで変わってしまうため、
	// 意図的に別関数として分離している)。
	Math::Vector3 GetForward() const { return transform_ ? transform_->GetForward() : Math::Vector3{}; }

	// プレイヤーの移動入力(ローカル軸)をワールド空間へ変換するための、
	// カメラ基準のyaw回転を返す。
	//   1. 自身の水平前方が十分な長さを持つならそれを基準にする
	//   2. ほぼ真上/真下を向いていて基準にできない場合は、CameraOrbitComponent
	//      (あれば)のyawにフォールバックする
	//   3. どちらも使えない(orbit_も無い)場合はQuaternion::Identity
	//      (=入力をワールド軸そのままとして扱う)を返す
	// 旧InputSystem::Update()にあった変換ロジックをここに集約したもの。
	Math::Quaternion GetMovementYawRotation() const {
		if (transform_ != nullptr) {
			Math::Vector3 forward = transform_->GetForward();
			forward.y = 0.0f;
			if (forward.LengthSquared() > kMinForwardLengthSq) {
				forward.Normalize();
				const float yaw = std::atan2(-forward.x, -forward.z);
				return Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw);
			}
		}

		if (orbit_ != nullptr) {
			return Math::Quaternion::CreateFromYawPitchRoll(orbit_->GetYaw(), 0.0f, 0.0f);
		}

		return Math::Quaternion::Identity;
	}

	KdCamera& GetCamera() { return *camera_; }

private:
	TransformComponent* transform_ = nullptr;
	CameraFollowComponent* follow_ = nullptr;
	CameraCollisionComponent* collision_ = nullptr;
	CameraShakeComponent* shake_ = nullptr;
	CameraOrbitComponent* orbit_ = nullptr;   // 同一GameObjectの兄弟、任意。移動方向フォールバック専用
	std::unique_ptr<KdCamera> camera_ = nullptr;

	// 自身の水平前方ベクトルがこれ以下(ほぼ真上/真下を向いている)の場合は、
	// そこからyawを決めずCameraOrbitComponent側にフォールバックする、という閾値。
	static constexpr float kMinForwardLengthSq = 1e-6f;
};