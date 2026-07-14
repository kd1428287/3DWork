#pragma once
#include "../Components/Camera/CameraComponent.h"
// ============================================================
// カメラの切り替え・シェイクなど、「Sceneに1つだけの、カメラに関する
// 動的な調整」を担うシステム。
//
// CameraComponent/CameraFollowComponentは1つのカメラの基本的な位置
// 計算だけに責務を絞っているため、
//   - 複数カメラの明示的な切り替え
//   - どのカメラが有効でも上から重ねたい効果(シェイク等)
// はどのコンポーネントにも自然な置き場がない。これらはGameObjectに
// 属さない"Sceneレベルの調整役"としてここに集約する。
//
// シェイクの起動は2経路:
//   - Shake()を直接呼ぶ (呼び出し元がCameraSystemへの参照を
//     既に持っている場合。対象が少数・既知のケースに向く)
//   - CameraShakeRequestEventをPublishする (攻撃・爆発・カード効果など
//     発生源が多数・多様な場合。CameraSystemが購読して反応する)
//
// 実行順序: ObjectManager::LateUpdate()(CameraFollowComponentが
// 基準位置を計算し終える)の"後"にCameraSystem::LateUpdate()を呼ぶ。
// CameraFollowComponentは毎フレーム対象+オフセットから位置を
// 再計算するだけなので、シェイクのオフセットが毎フレーム蓄積する
// 心配はない(その場限りの一時的な上乗せとして扱われる)。
// ============================================================
class CameraSystem {
public:
    CameraSystem(SceneContext& context) : context_(context) {
		shakeSub_ = context_.eventBus->subscribe<T>([this]() {});
			/*Subscribe<CameraShakeRequestEvent>([this](const CameraShakeRequestEvent& e) {
            Shake(e.intensity, e.duration);
        });*/
    }

    ~CameraSystem() {
        bus_.Unsubscribe<CameraShakeRequestEvent>(shakeSub_);
    }

    // 明示的にアクティブカメラを切り替える。
    // (CameraComponent::Start()での無条件上書きに頼らず、
    //  複数カメラを扱う場合はここを通す運用にする)
    void SwitchTo(CameraComponent* camera) { context_.activeCamera = camera; }

    // シェイクを要求する。既存のシェイクより弱い場合は上書きしない
    // (小さな揺れが、進行中の大きな揺れを打ち消してしまうのを防ぐ)。
    void Shake(float intensity, float duration) {
        if (remaining_ <= 0.0f || intensity >= intensity_) {
            intensity_ = intensity;
            duration_ = duration;
            remaining_ = duration;
        }
    }

    void PostUpdate(float deltaTime) {
        if (remaining_ <= 0.0f) return;

        CameraComponent* camera = context_.activeCamera;
        if (camera == nullptr) return;

        TransformComponent* transform = camera->GetOwner()->GetComponent<TransformComponent>();
        if (transform == nullptr) return;

        remaining_ -= deltaTime;
        const float t = remaining_ > 0.0f ? (remaining_ / duration_) : 0.0f;  // 1.0 -> 0.0 で減衰
        const float currentIntensity = intensity_ * t;

        transform->position = transform->position + RandomOffset(currentIntensity);
    }

private:
    static Math::Vector3 RandomOffset(float intensity) {
        auto randRange = []() { return (std::rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f; };
        return Math::Vector3{randRange(), randRange(), 0.0f} * intensity;
    }

    SceneContext& context_;
 
    SubscriptionId shakeSub_ = 0;

    float intensity_ = 0.0f;
    float duration_ = 0.0f;
    float remaining_ = 0.0f;
};
