// WireFrameComponent.h
#pragma once

#include "../Transform/TransformComponent.h"
#include "../Collision/ColliderComponent.h"
#include "../../../Framework/Utility/KdDebug/KdDebugWireFrame.h"

// ============================================================
// アタッチしたGameObjectのColliderComponentの形状を、毎フレーム
// 自動でワイヤーフレーム表示するコンポーネント。
//
// ModelRenderComponentがSkeletonComponentを自分でキャッシュして
// DrawModel()を呼ぶのと同じ構造: Start()でColliderComponent*を
// キャッシュし、DrawDebug()(IRenderableのデバッグ描画パス)から
// 自動で形状を描く。呼び出し側が明示的にDrawCollider()等を呼ぶ必要はない。
//
// KdDebugWireFrame本体(頂点バッファ)はGameObjectごとに1つ持つ
// (ModelRenderComponentが描画対象のモデルデータを自分のTransformで
// 描くのと同じく、他のGameObjectとバッファを共有しない)。
// ============================================================
class WireFrameComponent : public ComponentBase, public IRenderable
{
public:
	explicit WireFrameComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		collider_ = GetOwner()->GetComponent<ColliderComponent>();
		if (collider_ == nullptr) {
			std::printf(
				"[WireFrameComponent] warning: %s has no ColliderComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	// IRenderable: デバッグ描画パスでColliderComponentの形状を描き、
	// 描いたらすぐクリアする(KdDebugWireFrame::Draw()の既存挙動)。
	void DrawDebug() override
	{
		if (!enabled_ || collider_ == nullptr) return;

		for (const CollisionShapeEntry& shape : collider_->GetShapes()) {
			if (!shape.enabled) continue;

			const Math::Color col = shape.isTrigger ? triggerColor_ : solidColor_;

			switch (shape.shape) {
			case ColliderShape::Sphere:
				wire_.AddDebugSphere(
					collider_->GetShapeWorldCenter(shape), collider_->GetShapeWorldRadius(shape), col);
				break;

			case ColliderShape::Box: {
				const CollisionMath::OrientedBox obb = collider_->GetShapeWorldOBB(shape);
				Math::Matrix m = Math::Matrix::CreateFromQuaternion(obb.orientation);
				m.Translation(obb.center);
				wire_.AddDebugBox(m, obb.halfExtents, {}, true, col);
				break;
			}
			case ColliderShape::Capsule: {
				const CollisionMath::Capsule capsule = collider_->GetShapeWorldCapsule(shape);
				DrawCapsuleShape(capsule.start, capsule.end, capsule.radius, col);
				break;
			}
			default:
				// Mesh/Polygonは三角形数が多く毎フレーム描画コストが
				// 見合わないため未対応。
				break;
			}
		}

		wire_.Draw();
	}

	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

	// isTrigger=falseの形状(押し返しあり)とtrueの形状(重なり検知のみ)を
	// 見分けたいことが多いため、色を分けられるようにしておく。
	void SetSolidColor(const Math::Color& col) { solidColor_ = col; }
	void SetTriggerColor(const Math::Color& col) { triggerColor_ = col; }

private:
	void DrawCapsuleShape(const Math::Vector3& worldStart, const Math::Vector3& worldEnd,
		float worldRadius, const Math::Color& col)
	{
		wire_.AddDebugSphere(worldStart, worldRadius, col);
		wire_.AddDebugSphere(worldEnd, worldRadius, col);

		Math::Vector3 axis = worldEnd - worldStart;
		if (axis.LengthSquared() < 1e-8f) return;
		axis.Normalize();
		Math::Vector3 up = (std::abs(axis.y) < 0.99f) ? Math::Vector3::Up : Math::Vector3::Right;
		Math::Vector3 side = axis.Cross(up); side.Normalize();
		Math::Vector3 fwd = axis.Cross(side); fwd.Normalize();

		for (const Math::Vector3& dir : { side, -side, fwd, -fwd }) {
			wire_.AddDebugLine(worldStart + dir * worldRadius, worldEnd + dir * worldRadius, col);
		}
	}

	ColliderComponent* collider_ = nullptr;
	bool enabled_ = true;
	Math::Color solidColor_ = kGreenColor;
	Math::Color triggerColor_ = kRedColor;
	KdDebugWireFrame wire_;
};