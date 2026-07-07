#pragma once
#include "../Tags/ICameraTarget.h"
#include "../Transform/TransformComponent.h"

// ============================================================
// GameObjectを「カメラの追従対象になれる」ようにするコンポーネント。
//
// 判定/追従ロジックは一切持たない。TransformComponentから座標を
// 取り出して返すだけの、CardSelectionComponentやPlayerInputComponentと
// 同じ「データの置き場に徹する」設計。
//
// TransformComponentが存在しない場合はnullptrチェックの上で
// Vector3{}(原点)を返す(前回の実装のnullチェック漏れを修正)。
// ============================================================
class CameraTargetComponent : public ComponentBase, public ICameraTarget {
public:
    explicit CameraTargetComponent(GameObject* owner) : ComponentBase(owner) {}

    void Start() override {
        transform_ = GetOwner()->GetComponent<TransformComponent>();
    }

    Math::Vector3 GetTargetPosition() const override {
        return transform_ ? transform_->position : Math::Vector3{};
    }

	Math::Matrix GetTargetMatrix() const override {
		return transform_ ? transform_->matrix : Math::Matrix{};
	}

private:
    TransformComponent* transform_ = nullptr;
};
