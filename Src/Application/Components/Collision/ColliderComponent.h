#pragma once

#include "../Transform/TransformComponent.h"

// ============================================================
// 当たり判定の"形状"だけを表すコンポーネント。
//
// 判定ロジック(誰と誰が当たっているか)は一切持たない。
// CollisionSystem/RaycastSystemが毎フレーム(または要求時)に
// 全てのColliderComponentを集めて判定し、結果をEventBus経由で
// 通知する、あるいは呼び出し側へ直接返す(MovementComponentが
// 「動く処理」だけを持ち、「動いた結果どうなるか」に関知しないのと
// 同じ構造)。
//
// GameObjectは型ごとに1インスタンスしかコンポーネントを持てないため
// (GameObject.h参照)、1体のGameObjectに「地形用」「攻撃判定用」など
// 複数の当たり判定を持たせたい場合、ColliderComponentを複数付けるのではなく、
// このコンポーネント自身が名前付きで複数の形状(CollisionShapeEntry)を
// 保持する形にしている。KdColliderの「名前付きで複数形状を登録する」
// 考え方を、コンポーネント指向向けに引き継いだもの。
//
// 形状は今のところSphere/AABBの2種類のみ。回転を考慮する箱(OBB)や、
// メッシュ/ポリゴン単位の精密な当たり判定(KdModelCollision/
// KdPolygonCollision相当)は未対応(必要になったら形状の種類を
// 追加する形で拡張する)。
// ============================================================

enum class ColliderShape
{
	Sphere,
	AABB,
};

// 当たり判定の「用途」を表すビットフラグ。KdCollider::Typeの役割を
// 引き継ぐが、名前はこのプロジェクトの命名に合わせて付け直している。
// ColliderComponent::AddSphere/AddAABBのlayer引数、および
// CollisionShapeEntry::layer/collideMaskに使う。
namespace ColliderLayer
{
	constexpr uint32_t Ground = 1u << 0; // 地形。上に乗れる/地面判定に使う
	constexpr uint32_t Bump = 1u << 1; // 横方向の押し合い(壁・キャラ同士等)
	constexpr uint32_t Hitbox = 1u << 2; // 攻撃判定(形状ベース)
	constexpr uint32_t HitLine = 1u << 3; // 攻撃判定(レイベース。RaycastSystem向け)
	constexpr uint32_t Hurtbox = 1u << 4; // 食らい判定
	constexpr uint32_t Sight = 1u << 5; // 視界判定
	constexpr uint32_t EventArea = 1u << 6; // イベント用の任意領域
	constexpr uint32_t All = 0xFFFFFFFFu;
}

// 1つの当たり判定形状。ColliderComponentが名前付きで複数個保持する。
struct CollisionShapeEntry
{
	std::string name;
	ColliderShape shape = ColliderShape::Sphere;
	Math::Vector3 offset{};

	// Sphereはradiusのみ、AABBはhalfExtentsのみ参照する
	float radius = 0.5f;
	Math::Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };

	// この形状がどのレイヤーに属するか
	uint32_t layer = ColliderLayer::Bump;
	// この形状がどのレイヤーと判定を取るか(デフォルトは全レイヤー)
	uint32_t collideMask = ColliderLayer::All;

	// true: 押し返しなどの物理応答はせず、重なり検知(イベント)だけ行う。
	bool isTrigger = false;

	// true: 押し返しの対象から自分を除外する(地形・壁など、押されても
	// 動かないもの)。isTrigger=falseな形状同士が重なった時、
	// CollisionSystemが自動的に位置補正を行うが、isStatic=trueの側は
	// 動かさず、相手側だけを押し出す(詳細はCollisionSystem参照)。
	bool isStatic = false;

	bool enabled = true;

	// お互いのlayer/collideMaskの両方が噛み合っていれば判定対象とする
	// (片方だけが相手のマスクに一致していても、相手側が自分を無視する
	//  設定なら判定しない、という双方向チェック)。
	bool CanCollideWith(const CollisionShapeEntry& other) const {
		return (collideMask & other.layer) != 0
			&& (other.collideMask & layer) != 0;
	}
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

	// --- 形状の登録/削除 ------------------------------------------------

	CollisionShapeEntry& AddSphere(std::string_view name, float radius,
		const Math::Vector3& offset = {}, uint32_t layer = ColliderLayer::Bump) {
		CollisionShapeEntry entry;
		entry.name = name;
		entry.shape = ColliderShape::Sphere;
		entry.radius = radius;
		entry.offset = offset;
		entry.layer = layer;
		return AddOrReplace(std::move(entry));
	}

	CollisionShapeEntry& AddAABB(std::string_view name, const Math::Vector3& halfExtents,
		const Math::Vector3& offset = {}, uint32_t layer = ColliderLayer::Bump) {
		CollisionShapeEntry entry;
		entry.name = name;
		entry.shape = ColliderShape::AABB;
		entry.halfExtents = halfExtents;
		entry.offset = offset;
		entry.layer = layer;
		return AddOrReplace(std::move(entry));
	}

	void RemoveShape(std::string_view name) {
		shapes_.erase(
			std::remove_if(shapes_.begin(), shapes_.end(),
				[&](const CollisionShapeEntry& e) { return e.name == name; }),
			shapes_.end());
	}

	void SetShapeEnabled(std::string_view name, bool enabled) {
		if (CollisionShapeEntry* entry = FindShape(name)) {
			entry->enabled = enabled;
		}
	}

	CollisionShapeEntry* FindShape(std::string_view name) {
		auto it = std::find_if(shapes_.begin(), shapes_.end(),
			[&](const CollisionShapeEntry& e) { return e.name == name; });
		return (it != shapes_.end()) ? &(*it) : nullptr;
	}
	const CollisionShapeEntry* FindShape(std::string_view name) const {
		auto it = std::find_if(shapes_.begin(), shapes_.end(),
			[&](const CollisionShapeEntry& e) { return e.name == name; });
		return (it != shapes_.end()) ? &(*it) : nullptr;
	}

	const std::vector<CollisionShapeEntry>& GetShapes() const { return shapes_; }

	// --- 参照 --------------------------------------------------------

	Math::Vector3 GetWorldPosition() const {
		return transform_ ? transform_->GetPosition() : Math::Vector3::Zero;
	}

	// 指定した形状のワールド座標での中心点(GetWorldPosition() + offset)。
	Math::Vector3 GetShapeWorldCenter(const CollisionShapeEntry& entry) const {
		return GetWorldPosition() + entry.offset;
	}

	// 押し返し(位置補正)のためにCollisionSystemから呼ばれる。
	// TransformComponentを直接書き換えるが、これはCollisionSystemが
	// 「このフレームの移動は全て確定した後」に実行される前提のため、
	// MovementComponent/VelocityComponentとの競合は起きない
	// (それらは既にこのフレームの位置更新を終えている)。
	void Translate(const Math::Vector3& delta) {
		if (transform_ != nullptr) {
			transform_->Translate(delta);
		}
	}

private:
	CollisionShapeEntry& AddOrReplace(CollisionShapeEntry&& entry) {
		if (CollisionShapeEntry* existing = FindShape(entry.name)) {
			*existing = std::move(entry);
			return *existing;
		}
		shapes_.push_back(std::move(entry));
		return shapes_.back();
	}

	TransformComponent* transform_ = nullptr;
	std::vector<CollisionShapeEntry> shapes_;
};