#pragma once
#include "../Transform/TransformComponent.h"
#include "../Camera/CameraTargetComponent.h"
#include "../../Core/SceneContext.h"
#include "../../Core/Handle.h"
#include "../../Systems/Collision/RaycastSystem.h"

// ============================================================
// カメラの貫通防止を責務とするコンポーネント。

// --- 既知の制約 ---------------------------------------------------
//   - ピボット自体が地形の内部にめり込んでいる場合、レイが
//     「当たらない」と誤判定され、逆にカメラが壁を素通りする
//     (fail-openする)。未解決。
//   - 除外できる自己ヒット対象はpivotTargetの持ち主1体のみ。
//     プレイヤーに追従する別GameObjectの装備等は除外されない。
// ============================================================
class CameraCollisionComponent : public ComponentBase {
public:
	explicit CameraCollisionComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// カメラの太さの近似半径。壁際で細い柱の角をすり抜けにくくする
	// (Sphere/Capsule/Boxに対してのみ有効。地形(Mesh/Polygon)には
	// 効かない。RaycastSystem::SphereCastClosestのコメント参照)。
	void SetSweepRadius(float radius) { sweepRadius_ = radius; }

	// 別GameObjectのコンポーネントを参照するため、Handle<T>で受け取る。
	void SetPivotTarget(Handle<CameraTargetComponent> target) { pivotTarget_ = target; }

	// ピボットをターゲット原点から少し上に持ち上げる
	// (足元基準だと壁際で地面すれすれの視点になりやすいため)。
	void SetPivotOffset(const Math::Vector3& offset) { pivotOffset_ = offset; }

	// 壁面からどれだけ手前で止めるか(めり込み防止の余白)。
	void SetSkinWidth(float skin) { skinWidth_ = skin; }

	// 障害物が無くなった後、desiredへ戻る速度(m/s)。
	void SetPullOutSpeed(float speed) { pullOutSpeed_ = speed; }

	void PostUpdate(float deltaTime) override {
		if (transform_ == nullptr) return;

		CameraTargetComponent* pivotTarget = pivotTarget_.Resolve();
		if (pivotTarget == nullptr) return;

		const SceneContext* context = GetOwner()->GetContext();
		const ColliderRegistry* registry = context ? context->colliderRegistry : nullptr;
		if (registry == nullptr) return; // GroundSensorComponentと同じく、1フレーム目は判定をスキップ

		// 自己ヒット除外対象はここで導出する(独立フィールドとして
		// 別途保持しない)。
		GameObject* ignoreOwner = pivotTarget->GetOwner();

		const Math::Vector3 pivot = pivotTarget->GetTargetPosition() + pivotOffset_;
		const Math::Vector3 desired = transform_->GetPosition(); // Followが決めた理想位置

		const Math::Vector3 toDesired = desired - pivot;
		const float desiredDistance = toDesired.Length();
		float targetDistance = desiredDistance;

		if (desiredDistance > 1e-4f) {
			const Math::Vector3 dir = toDesired / desiredDistance;

			RaycastSystem::Hit hit;
			const bool blocked = RaycastSystem::SphereCastClosest(
				*registry, pivot, dir, desiredDistance, sweepRadius_,
				ColliderCategory::Ground | ColliderCategory::Bump,
				hit, ignoreOwner);

			if (blocked) {
				targetDistance = std::max(0.0f, hit.result.distance - skinWidth_);
			}
		}
		else {
			targetDistance = 0.0f;
		}

		// 近づく方向は瞬時に反映(めり込みを確実に防ぐ)。
		// 離れる方向(障害物解消後にdesiredへ戻す)だけ緩やかに補間する。
		if (targetDistance < currentDistance_ || currentDistance_ < 0.0f) {
			currentDistance_ = targetDistance;
		}
		else {
			currentDistance_ = std::min(targetDistance,
				currentDistance_ + pullOutSpeed_ * deltaTime);
		}

		const Math::Vector3 finalDir = (desiredDistance > 1e-4f) ? (toDesired / desiredDistance) : Math::Vector3::Zero;
		transform_->SetPosition(pivot + finalDir * currentDistance_);
	}

private:
	TransformComponent* transform_ = nullptr;
	Handle<CameraTargetComponent> pivotTarget_;

	Math::Vector3 pivotOffset_{ 0.0f, 1.0f, 0.0f };
	float skinWidth_ = 0.3f;
	float pullOutSpeed_ = 8.0f;
	float currentDistance_ = -1.0f; // 未初期化フラグ
	float sweepRadius_ = 0.15f;
};