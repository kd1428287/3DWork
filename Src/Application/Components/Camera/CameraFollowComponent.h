#pragma once
#include "../Tags/ICameraTarget.h"
#include "../Transform/TransformComponent.h"
#include "CameraOrbitComponent.h"

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
//
// CameraOrbitComponent(マウスによる軌道回転)が同じGameObjectに
// 付いている場合、オフセットの回転には対象の向きではなくこちらの
// 軌道角度を使う。これにより「対象がどちらを向いていても、
// マウスでカメラだけ自由に回せる」三人称カメラになる(任意添付。
// 無ければ従来通り対象の向きにオフセットが追従する)。
// ============================================================
class CameraFollowComponent : public ComponentBase {
public:
	explicit CameraFollowComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// 無くてもよい(任意)。存在する場合のみマウス軌道回転を優先して使う。
		orbit_ = GetOwner()->GetComponent<CameraOrbitComponent>();
	}

	void SetTarget(ICameraTarget* target) { target_ = target; }
	void SetLocalOffset(const Math::Vector3& offset) { localOffset_ = offset; }

	// 追従対象の向きにもカメラを合わせたい場合はtrue(三人称カメラ等)。
	// オフセットだけ追従先の向きに乗せて、カメラ自身の向きは
	// 別のロジック(LookAt等)に任せたい場合はfalseにする。
	void SetFollowRotation(bool follow) { followRotation_ = follow; }

	// Update(移動・入力解決)が全て終わった後に追従先を読みたいため、
	// PostUpdateで計算する(移動した直後の対象を"遅れて"追う、
	// という映像的な違和感を避けられる)。
	void PostUpdate(float /*deltaTime*/) override {
		if (transform_ == nullptr || target_ == nullptr) return;

		// CameraOrbitComponentがあればマウス軌道回転を、無ければ従来通り
		// 対象自身の向きをオフセットの回転基準にする。
		const Math::Quaternion offsetRotation =
			(orbit_ != nullptr) ? orbit_->GetOrbitRotation() : target_->GetTargetRotation();

		const Math::Vector3 targetPos = target_->GetTargetPosition();

		// ローカルオフセットを回転基準で回転させ、ワールド空間のオフセットにする。
		const Math::Vector3 worldOffset =
			Math::Vector3::Transform(localOffset_, offsetRotation);

		transform_->SetPosition(targetPos + worldOffset);

		if (followRotation_) {
			transform_->SetRotation(offsetRotation);
		}
	}

private:
	TransformComponent* transform_ = nullptr;
	CameraOrbitComponent* orbit_ = nullptr; // 無くてもよい(任意)
	ICameraTarget* target_ = nullptr;
	Math::Vector3 localOffset_{ 0.0f, 0.0f, -10.0f };
	bool followRotation_ = true;
};