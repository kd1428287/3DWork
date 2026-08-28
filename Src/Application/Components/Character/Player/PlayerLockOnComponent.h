#pragma once
#include "../Enemy/LockOnTargetComponent.h"
#include "PlayerCombatTypes.h" // kDirectionEpsilon
#include "../../Camera/CameraComponent.h"
#include "../../Transform/TransformComponent.h"
#include "../../../Core/Handle.h"

// ============================================================
// ロックオン対象の「選定」を担当するコンポーネント。
// PlayerStatusControllerの兄弟コンポーネントとしてPlayerにアタッチする。
//
// 現在ロック中の対象そのもの(状態)はSceneContext::lockedTargetに
// 集約している(activeCameraと同じ「シーンに1つだけの既知の対象」置き場
// という既存の方針に沿う)。このコンポーネントは「誰をロックするか」を
// 決める選定ロジックと、その結果をSceneContextへ書き込む処理だけを持ち、
// 状態そのものは持たない。これにより、カメラ側(CameraOrbitComponent等)や
// 将来増えるかもしれない他のシステムも、このクラスの型を知らずに
// SceneContext::lockedTargetを読むだけでロック対象を参照できる。
//
// 選定ロジック(FindNearestToScreenCenter)は状態を変更しない問い合わせ
// 関数として公開しており、"lock"入力によるロック確定(TryLockOn)だけでなく、
// 未ロック時の攻撃対象決定(PlayerStatusController::FaceAttackTarget)にも
// そのまま使い回す。
//
// レイキャストではなくカメラ前方ベクトルとの内積でランキングする方式を
// 採用している(複数候補から「画面中心に一番近いもの」を選ぶ用途には
// 一点判定のレイより向いており、View/Projection行列も不要なため)。
//
// EnemyStatusController/HealthComponent等、敵側の内部実装は一切知らず
// LockOnTargetComponentの有無とIsLockable()だけを見て判定する。
// ============================================================
class PlayerLockOnComponent : public ComponentBase
{
public:
	explicit PlayerLockOnComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		// SceneContext経由でアクティブカメラを取得する
		// (GameObject::GetContext()参照。Sceneに1つだけの既知の対象は
		//  ここにまとめる、という既存の方針に沿う)。
		sceneContext_ = GetOwner()->GetContext();
		if (sceneContext_ != nullptr) {
			cameraComponent_ = sceneContext_->activeCamera;
		}
	}

	void Update(float /*deltaTime*/) override {
		if (sceneContext_ == nullptr) return;

		// ロック中の対象が死亡/射程外になったら自動解除する。
		if (GameObject* target = sceneContext_->lockedTarget.Resolve()) {
			LockOnTargetComponent* targetComp = target->GetComponent<LockOnTargetComponent>();
			if (targetComp == nullptr || !targetComp->IsLockable() || !IsWithinRange(target)) {
				sceneContext_->lockedTarget = {};
			}
		}
	}

	// "lock"入力から呼ばれる。既にロック中の場合の挙動(維持/切り替え/解除)は
	// 呼び出し側(PlayerStatusController::HandleActionInput)のポリシーに委ねる。
	void TryLockOn() {
		if (sceneContext_ == nullptr) return;
		if (GameObject* nearest = FindNearestToScreenCenter()) {
			sceneContext_->lockedTarget = Handle<GameObject>(nearest);
		}
	}

	void ClearLockOn() {
		if (sceneContext_ != nullptr) sceneContext_->lockedTarget = {};
	}
	bool IsLockedOn() const {
		return sceneContext_ != nullptr && sceneContext_->lockedTarget.Resolve() != nullptr;
	}
	GameObject* GetLockedTarget() const {
		return sceneContext_ != nullptr ? sceneContext_->lockedTarget.Resolve() : nullptr;
	}

	// 画面中心に最も近いロック可能対象を検索するだけの関数(状態は変更しない)。
	GameObject* FindNearestToScreenCenter() const {
		if (cameraComponent_ == nullptr || sceneContext_ == nullptr || sceneContext_->objectManager == nullptr) return nullptr;

		const Math::Vector3 camPos = cameraComponent_->GetPosition();
		Math::Vector3 camForward = -cameraComponent_->GetForward();
		camForward.Normalize();

		GameObject* best = nullptr;
		float bestDot = kLockOnHalfFovCos; // これより中心寄りのものだけ候補にする

		// ObjectManager::FindComponents<T>()が既にシーン走査を提供しているため、
		// 独自のレジストリ登録は持たない。
		for (LockOnTargetComponent* target : sceneContext_->objectManager->FindComponents<LockOnTargetComponent>()) {
			if (!target->IsLockable()) continue;

			Math::Vector3 toTarget = target->GetReticlePosition() - camPos;
			const float distSq = toTarget.LengthSquared();
			if (distSq > kLockOnRange * kLockOnRange || distSq <= kDirectionEpsilon) continue;

			toTarget.Normalize();
			const float dot = camForward.Dot(toTarget);
			if (dot > bestDot) {
				bestDot = dot;
				best = target->GetOwner();
			}
		}
		return best;
	}

private:
	bool IsWithinRange(GameObject* target) const {
		if (cameraComponent_ == nullptr) return false;
		TransformComponent* transform = target->GetComponent<TransformComponent>();
		if (transform == nullptr) return false;
		const float distSq = (transform->GetPosition() - cameraComponent_->GetPosition()).LengthSquared();
		return distSq <= kLockOnRange * kLockOnRange;
	}

	CameraComponent* cameraComponent_ = nullptr;

	// ロック状態そのものはここでは保持しない。SceneContext::lockedTarget
	// (activeCameraと同じ「シーンに1つだけの既知の対象」置き場)を唯一の
	// 保持場所とし、このコンポーネントは選定ロジックとその書き込みだけを
	// 担当する。これにより、カメラ側のCameraOrbitComponent等も
	// PlayerLockOnComponentという型を知らずにSceneContext::lockedTargetを
	// 読むだけでロック対象を参照できるようになる。
	SceneContext* sceneContext_ = nullptr;

	// 【要調整】仮の値。実際の画角・カメラ距離感に合わせて調整する前提。
	static constexpr float kLockOnRange = 150.0f;
	static constexpr float kLockOnHalfFovCos = 0.f; // 目安: 前方±60度くらいを候補とする
};