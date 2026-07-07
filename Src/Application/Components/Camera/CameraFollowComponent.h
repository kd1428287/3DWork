#pragma once
#include "../Tags/ICameraTarget.h"
#include "../Transform/TransformComponent.h"

// ============================================================
// カメラの追従"ロジック"だけを持つコンポーネント。
//
// CameraComponent(「これがカメラである」ことを表すだけ)とは完全に
// 独立しており、お互いの存在を知らない。両者は同じGameObjectの
// TransformComponentを介してのみやり取りする
// (MovementComponentがTransformを書き換え、他のコンポーネントが
//  それを読むのと全く同じ構造)。
//
// これにより:
//   - 追従しない固定カメラにしたい  -> このコンポーネントを付けない
//   - 追従対象を切り替えたい        -> SetTarget()を呼び替えるだけ
//   - 追従ロジック自体を差し替えたい -> 別のFollow系コンポーネントに
//                                       挿げ替えるだけ
// が、CameraComponent側を一切変更せずに実現できる。
//
// 追従対象(ICameraTarget)は生成時/設定時に直接指定するPush方式。
// カメラの追従対象は通常1つ(プレイヤー等)で、生成タイミングも
// 決まっているため、シーン全体を毎フレーム検索するより効率的かつ、
// 「今どこを追っているか」がコード上明示的で分かりやすい。
// ============================================================
class CameraFollowComponent : public ComponentBase {
public:
    explicit CameraFollowComponent(GameObject* owner) : ComponentBase(owner) {}

    void Start() override {
        transform_ = GetOwner()->GetComponent<TransformComponent>();
    }

    void SetTarget(ICameraTarget* target) { target_ = target; }
    void SetLocalOffset(const Math::Vector3& offset) { localOffset_ = offset; }

    // Update(移動・入力解決)が全て終わった後に追従先を読みたいため、
    // LateUpdateで計算する(移動した直後のプレイヤーを"遅れて"追う、
    // という映像的な違和感を避けられる)。
    void PostUpdate(float /*deltaTime*/) override {
        if (transform_ == nullptr || target_ == nullptr) return;

		Math::Matrix mat = target_->GetTargetMatrix();
		mat.Up().Normalize();
		mat.Right().Normalize();
		mat.Forward().Normalize();

		Math::Vector3 worldOffset = Math::Vector3::Transform(
			localOffset_, Math::Quaternion::CreateFromRotationMatrix(mat));
	
        const Math::Vector3 targetPos = target_->GetTargetPosition();
        transform_->position = {
            targetPos.x + worldOffset.x,
            targetPos.y + worldOffset.y,
            targetPos.z + worldOffset.z,
        };
    }

private:
    TransformComponent* transform_ = nullptr;
    ICameraTarget* target_ = nullptr;
    Math::Vector3 localOffset_{0.0f, 0.0f, -10.0f};
};
