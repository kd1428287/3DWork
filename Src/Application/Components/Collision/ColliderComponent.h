#pragma once
#include <cstdint>
#include <cstdio>

#include "../Transform/TransformComponent.h"

// ============================================================
// 当たり判定の"形状"だけを表すコンポーネント。
//
// 判定ロジック(誰と誰が当たっているか)は一切持たない。
// CollisionSystemが毎フレーム全てのColliderComponentを集めて
// 総当たりチェックし、結果をLocalEventBus経由で通知する
// (MovementComponentが「動く処理」だけを持ち、「動いた結果どうなるか」に
//  関知しないのと同じ構造)。
//
// 形状は今のところSphere/AABBの2種類のみ。ポリモーフィックな
// Shapeクラス階層にはまだしていない
// (以前CombatStateをenum+switchで始めた時と同じ判断: 種類が
//  少ないうちはenumで十分。形状の種類が増えて分岐が複雑になったら
//  Strategy的な構成へ移行する)。
//
// 回転は考慮しない(AABBは常に軸並行)。向きを持つ箱(OBB)が
// 必要になったら、その時に別の形状として追加する。
// ============================================================
enum class ColliderShape
{
	Sphere,
	AABB,
};

enum class CollisionType
{
	CollisionBody = 1 << 0,
	Hitbox = 1 << 1,
	Terrain = 1 << 2,
};

class ColliderComponent : public ComponentBase
{
public:
	explicit ColliderComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ == nullptr) {
			std::printf(
				"[ColliderComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	// --- 形状の設定 ----------------------------------------------------

	void SetAsSphere(float radius, const Math::Vector3& offset = {}) {
		shape_ = ColliderShape::Sphere;
		radius_ = radius;
		offset_ = offset;
	}

	void SetAsAABB(const Math::Vector3& halfExtents, const Math::Vector3& offset = {}) {
		shape_ = ColliderShape::AABB;
		halfExtents_ = halfExtents;
		offset_ = offset;
	}

	// --- 参照 ------------------------------------------------------------

	ColliderShape GetShape() const { return shape_; }
	float GetRadius() const { return radius_; }
	const Math::Vector3& GetHalfExtents() const { return halfExtents_; }

	// TransformComponent::position + offset_ のワールド座標。
	// TransformComponentが無い場合はoffset_だけを返す(原点基準)。
	Math::Vector3 GetWorldCenter() const {
		const Math::Vector3 base = transform_ ? transform_->GetPosition() : Math::Vector3::Zero;
		return base + offset_;
	}

	// --- トリガー/レイヤー --------------------------------------------

	// true: 押し返しなどの物理応答はせず、重なり検知(イベント)だけ行う。
	// 今回のCollisionSystemはそもそも押し返しを実装していないため、
	// 現状は意味づけ以外の実効果は無い(物理応答を実装する時のための
	// フラグとして先に用意している)。
	void SetTrigger(bool isTrigger) { isTrigger_ = isTrigger; }
	bool IsTrigger() const { return isTrigger_; }

	// 自分がどのレイヤーに属するか(ビットフラグ)。
	void SetLayer(uint32_t layer) { layer_ = layer; }
	uint32_t GetLayer() const { return layer_; }

	// 自分がどのレイヤーと判定を取るか。デフォルトは全レイヤーと判定する。
	void SetCollideMask(uint32_t mask) { collideMask_ = mask; }
	uint32_t GetCollideMask() const { return collideMask_; }

	// お互いのレイヤー/マスクの両方が噛み合っていれば判定対象とする
	// (片方だけがtargetのマスクに一致していても、target側が自分を
	//  無視する設定なら判定しない、という双方向チェック)。
	bool CanCollideWith(const ColliderComponent& other) const {
		return (collideMask_ & other.layer_) != 0
			&& (other.collideMask_ & layer_) != 0;
	}

private:
	TransformComponent* transform_ = nullptr;

	ColliderShape shape_ = ColliderShape::Sphere;
	Math::Vector3 offset_{};

	float radius_ = 0.5f;
	Math::Vector3 halfExtents_{ 0.5f, 0.5f, 0.5f };

	bool isTrigger_ = false;
	uint32_t layer_ = 0x1;
	uint32_t collideMask_ = 0xFFFFFFFF;
};
