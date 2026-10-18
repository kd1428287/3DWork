#pragma once

#include "../../Physics/Collision/ColliderComponent.h"

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

	void Awake() override
	{
		collider_ = GetOwner()->GetComponent<ColliderComponent>();
		if (collider_ == nullptr) {
			std::printf(
				"[WireFrameComponent] warning: %s has no ColliderComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	// IRenderable: デバッグ描画パスでColliderComponentの形状を描き、すぐクリアする
	void DrawDebug() override
	{
		if (!enabled_ || collider_ == nullptr) return;

		const Math::Matrix world = collider_->GetWorldMatrix();
		for (const CollisionShapeEntry& shape : collider_->GetShapes()) {
			if (!shape.enabled) continue;
			AddShapeWire(wire_, shape, world, shape.isTrigger ? triggerColor_ : solidColor_);
		}

		wire_.Draw();
	}

	// 形状1つをワールド行列基準でワイヤーへ追加する。MapEditorのプレビューとも共用
	static void AddShapeWire(KdDebugWireFrame& wire, const CollisionShapeEntry& shape,
		const Math::Matrix& world, const Math::Color& col)
	{
		Math::Matrix w = world;	// Decomposeが非constのためコピー
		Math::Vector3 scale, pos;
		Math::Quaternion rot;
		w.Decompose(scale, rot, pos);
		const float maxScale = std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });

		switch (shape.shape) {
		case ColliderShape::Sphere:
			wire.AddDebugSphere(Math::Vector3::Transform(shape.offset, world), shape.radius * maxScale, col);
			break;

		case ColliderShape::Box: {
			Math::Matrix m = Math::Matrix::CreateFromQuaternion(rot);
			m.Translation(Math::Vector3::Transform(shape.offset, world));
			const Math::Vector3 he(
				shape.halfExtents.x * std::abs(scale.x),
				shape.halfExtents.y * std::abs(scale.y),
				shape.halfExtents.z * std::abs(scale.z));
			wire.AddDebugBox(m, he, {}, true, col);
			break;
		}
		case ColliderShape::Capsule:
			DrawCapsuleShape(wire,
				Math::Vector3::Transform(shape.offset, world),
				Math::Vector3::Transform(shape.capsuleEnd, world),
				shape.radius * maxScale, col);
			break;

		default:
			break;	// Mesh/Polygonは三角形数が多いため未対応
		}
	}

	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

	// isTrigger=falseの形状(押し返しあり)とtrueの形状(重なり検知のみ)を
	// 見分けたいことが多いため、色を分けられるようにしておく。
	void SetSolidColor(const Math::Color& col) { solidColor_ = col; }
	void SetTriggerColor(const Math::Color& col) { triggerColor_ = col; }

private:
	static void DrawCapsuleShape(KdDebugWireFrame& wire, const Math::Vector3& worldStart,
		const Math::Vector3& worldEnd, float worldRadius, const Math::Color& col)
	{
		wire.AddDebugSphere(worldStart, worldRadius, col);
		wire.AddDebugSphere(worldEnd, worldRadius, col);

		Math::Vector3 axis = worldEnd - worldStart;
		if (axis.LengthSquared() < 1e-8f) return;
		axis.Normalize();
		Math::Vector3 up = (std::abs(axis.y) < 0.99f) ? Math::Vector3::Up : Math::Vector3::Right;
		Math::Vector3 side = axis.Cross(up); side.Normalize();
		Math::Vector3 fwd = axis.Cross(side); fwd.Normalize();

		for (const Math::Vector3& dir : { side, -side, fwd, -fwd }) {
			wire.AddDebugLine(worldStart + dir * worldRadius, worldEnd + dir * worldRadius, col);
		}
	}

	ColliderComponent* collider_ = nullptr;
	bool enabled_ = true;
	Math::Color solidColor_ = kGreenColor;
	Math::Color triggerColor_ = kRedColor;
	KdDebugWireFrame wire_;
};