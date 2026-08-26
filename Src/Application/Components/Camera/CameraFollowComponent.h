#pragma once
#include "../Tags/ICameraTarget.h"
#include "../Transform/TransformComponent.h"
#include "../Camera/CameraTargetComponent.h"
#include "../../Core/Handle.h"
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

	// Update(移動・入力解決)が全て終わった後に追従先を読みたいため、
	// PostUpdateで計算する。
	void PostUpdate(float /*deltaTime*/) override {
		if (transform_ == nullptr) return;

		// 毎フレームResolve()で有効性を確認する。対象がすでに破棄されて
		// いれば単にnullptrが返るだけで、以前のようにダングリング
		// ポインタを触ってUBになることがない。
		CameraTargetComponent* target = target_.Resolve();
		if (target == nullptr) return;

		// CameraOrbitComponentがあればマウス軌道回転を、無ければ従来通り
		// 対象自身の向きをオフセットの回転基準にする。
		const Math::Quaternion offsetRotation =
			(orbit_ != nullptr) ? orbit_->GetOrbitRotation() : target->GetTargetRotation();

		const Math::Vector3 targetPos = target->GetTargetPosition();

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
	CameraOrbitComponent* orbit_ = nullptr;        // 同一GameObjectの兄弟コンポーネントなので生ポインタのまま
	Handle<CameraTargetComponent> target_;         // 別GameObjectの参照なのでHandle化
	Math::Vector3 localOffset_{ 0.0f, 0.0f, -10.0f };
	bool followRotation_ = true;
};