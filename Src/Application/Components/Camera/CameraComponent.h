#pragma once
#include "../Transform/TransformComponent.h"

// ============================================================
// カメラを表すコンポーネント。
//
// 「これがカメラである」ことと「座標をどう公開するか」だけに
// 責務を絞っており、追従ロジックは一切持たない。
// 追従させたい場合はCameraFollowComponentを同じGameObjectに
// 追加で付ける(このクラス自体はCameraFollowComponentの存在を知らない。
// 両者は同じGameObjectのTransformComponentを介してのみ繋がる)。
//
// Start()の時点でSceneContext::activeCameraに自分自身を登録する。
// これにより、他のどのGameObjectのコンポーネントからも
//   GetOwner()->GetContext()->activeCamera
// という一貫した経路でカメラ情報にアクセスできる。
//
// 「カメラがどのGameObjectか」は生成時に固定される既知の関係では
// なく後から切り替わりうる(カットシーン用カメラへの切り替え等)ため、
// 生成時に一度だけ渡すPush方式ではなく、SceneContextという
// 共有の"今のアクティブカメラ"置き場を都度参照する方式にしている。
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

	void PostUpdate(float dt)override
	{
		camera_->SetCameraMatrix(transform_->GetWorldMatrix());
		//camera_->GetCamera().SetCameraMatrix(Math::Matrix::Identity);
		camera_->SetToShader();
	}

    Math::Vector3 GetPosition() const { return transform_ ? transform_->position : Math::Vector3{}; }
	KdCamera& GetCamera() const { return *camera_; }

private:
    TransformComponent* transform_ = nullptr;
	std::unique_ptr<KdCamera>					camera_ = nullptr;
};
