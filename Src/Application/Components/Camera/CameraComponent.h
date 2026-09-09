#pragma once
#include "../Transform/TransformComponent.h"
#include "CameraFollowComponent.h"
#include "CameraCollisionComponent.h"
#include "CameraShakeComponent.h"

// ============================================================
// カメラを表すコンポーネント。
//
// 責務は以下の3つ:
//   1. 「これがカメラである」ことの管理(SceneContext::activeCameraへの登録)
//   2. 外部への正対ベクトル/座標等のアクセサ提供
//   3. 同一GameObject上のカメラ関連コンポーネント(Follow/Collision/Shake)の
//      実行順序の保証
//
// 3.が必要な理由:
//   Follow  … 理想位置を決める
//   Collision … それを壁際で補正する(内部にcurrentDistance_という
//               フレームをまたぐ平滑化状態を持つため、Followより後、
//               かつShakeより前に実行しなければならない)
//   Shake   … Collisionが確定させた位置・向きに対してオフセットを
//               "加算するだけ"(Collisionの平滑化状態にシェイクの揺れが
//               混入すると誤動作するため、必ず最後に加算する)
// という一方向の依存があるが、ComponentBase::PostUpdateの自動呼び出し
// だけでは兄弟コンポーネント間の実行順序が保証されない。カメラという
// 概念の代表であるこのクラスが明示的に順序を握ることで、依存関係を
// コードとして可視化する。
//
// そのためFollowComponent/CollisionComponent/ShakeComponentは、
// ComponentBase::PostUpdateをoverrideしていない(Resolve()という
// ただのメソッドになっている)。ObjectManagerの自動巡回では呼ばれず、
// 必ずこのクラス経由でのみ呼ばれる。付けなくても動くが、その場合は
// 該当の機能が一切効かないだけで、事故(二重実行や無関係な自動呼び出し)
// は起きない。
//
// Shakeだけはヒットストップ(TimeScaleSystemによるdeltaTime減速)中も
// 揺れを止めたくないという演出判断のため、意図的にSceneContext::
// unscaledDeltaTime(スケールの影響を受けない生のdeltaTime)を使う。
// Follow/Collisionにはスケール済みdeltaTimeを渡す。
//
// 【要対応】この変更にはSceneContext.hへの
//     float unscaledDeltaTime = 0.0f;
// の追加が必要(このファイルからは編集不可のため未反映)。毎フレーム、
// TimeScaleSystem適用前の生deltaTimeで更新すること。
//
// 描画時のカメラ行列反映(SetCameraMatrix/SetToShader)はこのクラスでは
// 行わない。CameraViewComponent::PreDraw()に一本化されている
// (以前はここにも同じ処理があり、CameraViewComponentと二重に実行
// されうる状態だったため統合した)。
// ============================================================
class CameraComponent : public ComponentBase {
public:
	explicit CameraComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		if (!camera_)
		{
			camera_ = std::make_unique<KdCamera>();
		}
		camera_->SetProjectionMatrix(60);
	}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// いずれも任意(無くてもよい)。付いていれば、このクラスが
		// まとめて実行順序を保証する。
		follow_ = GetOwner()->GetComponent<CameraFollowComponent>();
		collision_ = GetOwner()->GetComponent<CameraCollisionComponent>();
		shake_ = GetOwner()->GetComponent<CameraShakeComponent>();

		if (auto* ctx = GetOwner()->GetContext()) {
			ctx->activeCamera = this;  // 自分をアクティブカメラとして登録
		}
	}

	void OnDestroy() override {
		// 自分が今もアクティブカメラのままなら、登録を解除しておく
		// (解除しないと、破棄後のポインタが残ってしまう)
		if (auto* ctx = GetOwner()->GetContext()) {
			if (ctx->activeCamera == this) {
				ctx->activeCamera = nullptr;
			}
		}
	}

	// Follow→Collision→Shakeの順で解決する。理由はクラス冒頭コメント参照。
	void PostUpdate(float deltaTime) override
	{
		if (follow_)    follow_->Resolve(deltaTime);
		if (collision_) collision_->Resolve(deltaTime);

		if (shake_ && transform_) {
			const SceneContext* ctx = GetOwner()->GetContext();
			// ctxが無い(通常は起こらないはずの)フレームのみdeltaTimeにフォールバック。
			const float shakeDt = ctx ? ctx->unscaledDeltaTime : deltaTime;

			shake_->Resolve(shakeDt);
			transform_->SetPosition(transform_->GetPosition() + shake_->GetPositionOffset());
			transform_->SetRotation(transform_->GetRotation() * shake_->GetRotationOffset());
		}
	}

	Math::Vector3 GetPosition() const { return transform_ ? transform_->GetPosition() : Math::Vector3{}; }

	// カメラの向いている方向(正規化済み)。CameraViewComponent::PreDraw()で
	// transform_->GetWorldMatrix()をそのままカメラ行列に渡しているため、
	// transform_->GetForward()がカメラの視線方向と一致する
	// (PlayerLockOnComponent::FindNearestToScreenCenter()が
	// 画面中心からの近さを判定するのに使う)。
	Math::Vector3 GetForward() const { return transform_ ? transform_->GetForward() : Math::Vector3{}; }

	KdCamera& GetCamera() { return *camera_; }

private:
	TransformComponent* transform_ = nullptr;
	CameraFollowComponent* follow_ = nullptr;       // 同一GameObjectの兄弟、任意
	CameraCollisionComponent* collision_ = nullptr; // 同一GameObjectの兄弟、任意
	CameraShakeComponent* shake_ = nullptr;         // 同一GameObjectの兄弟、任意
	std::unique_ptr<KdCamera> camera_ = nullptr;
};