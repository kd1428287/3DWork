#pragma once
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include "../Components/Collision/ColliderComponent.h"
#include "../Core/Handle.h"

// ============================================================
// 当たり判定システム(基礎実装)。
//
// InputSystem/CameraSystemと同じ「シーンに1つ、外側から明示的に
// Update()を呼んでもらう」System。ただし対象コライダーは固定登録では
// なく、毎フレームObjectManager::FindComponents<ColliderComponent>()で
// 動的に集める(登録制だと、動的に増減する対象と相性が悪いため。
// EffectResolverSystemがイベントバス経由にしたのと同じ理由)。
//
// 今回のスコープ(意図的に簡略化している点。改良時の見取り図として
// 明記しておく):
//   - ブロードフェーズ無し。全ペアを総当たりでチェックする(O(n^2))。
//     コライダー数が増えたら空間分割(グリッド/BVH等)の導入を検討する。
//   - 押し返し(位置補正)は行わない。重なりの検知とイベント通知のみ。
//   - 連続衝突検出(トンネリング対策)は無い。1フレームで貫通するほど
//     速いオブジェクトがあれば当たり判定をすり抜ける。
//   - Sphere-Sphere / Sphere-AABB / AABB-AABB の3パターンのみ対応。
//   - 重なりペアの記録にはComponentHandle<ColliderComponent>を使っている。
//     生ポインタのままだと、破棄されたコンポーネントのヒープ領域が
//     同じフレーム内で別の新しいコンポーネントに再利用された場合、
//     「まだ生きているポインタに見えるが実は別物」という事故(ABA問題)を
//     招く。世代番号付きのハンドルで、そのすり替わりを確実に検出する。
// ============================================================
class CollisionSystem {
public:
	// Events::Collision::～ と毎回書かずに済むよう、クラス内だけに限定した
	// using宣言(ヘッダ全体やincludeした側の名前空間を汚さないため)。
	using CollisionEnterEvent = Events::Collision::CollisionEnterEvent;
	using CollisionExitEvent = Events::Collision::CollisionExitEvent;

	// ObjectManagerから存在する全ColliderComponentを集め、総当たりで
	// 重なりをチェックする。Scene側の「このフレームの移動は全て確定した」
	// タイミング(ObjectManager::Update()の後、Flush()の前あたり)で
	// 呼ぶことを想定している。
	void Update(ObjectManager& objectManager) {
		const std::vector<ColliderComponent*> colliders =
			objectManager.FindComponents<ColliderComponent>();

		std::unordered_set<PairKey, PairKeyHash> currentOverlaps;

		for (size_t i = 0; i < colliders.size(); ++i) {
			ColliderComponent* a = colliders[i];
			if (!IsCollidable(a)) continue;

			for (size_t j = i + 1; j < colliders.size(); ++j) {
				ColliderComponent* b = colliders[j];
				if (!IsCollidable(b)) continue;

				// 同じGameObjectに複数コライダーが付いている場合、自分同士は判定しない
				if (a->GetOwner() == b->GetOwner()) continue;

				if (!a->CanCollideWith(*b)) continue;
				if (!Overlaps(*a, *b)) continue;

				const PairKey key = MakeKey(a, b);
				currentOverlaps.insert(key);

				// 前フレームまで重なっていなければ「新しく重なった」= Enter
				if (previousOverlaps_.find(key) == previousOverlaps_.end()) {
					PublishEnter(a, b);
				}
			}
		}

		// 前フレームは重なっていたが今回は重なりが消えたペア = Exit
		for (const PairKey& key : previousOverlaps_) {
			if (currentOverlaps.find(key) != currentOverlaps.end()) continue; // まだ重なっている

			// ComponentHandle::Resolve()が世代番号まで検証してくれるため、
			// 「破棄後にポインタ値だけ再利用された別のコンポーネント」を
			// 誤って生存扱いすることなく、安全にnullptrとして弾ける。
			ColliderComponent* a = key.first.Resolve();
			ColliderComponent* b = key.second.Resolve();
			if (a == nullptr || b == nullptr) continue;

			PublishExit(a, b);
		}

		previousOverlaps_ = std::move(currentOverlaps);
	}

private:
	using ColliderHandle = ComponentHandle<ColliderComponent>;
	using PairKey = std::pair<ColliderHandle, ColliderHandle>;

	struct PairKeyHash {
		size_t operator()(const PairKey& key) const {
			const size_t h1 = std::hash<ColliderHandle>{}(key.first);
			const size_t h2 = std::hash<ColliderHandle>{}(key.second);
			return h1 ^ (h2 << 1);
		}
	};

	static PairKey MakeKey(ColliderComponent* a, ColliderComponent* b) {
		// 順序はポインタ値で正規化する(A-B/B-Aで別キーにならないようにするため)。
		// ハンドル自体の同一性比較には影響しない(世代番号もそれぞれ保持される)。
		return (a < b) ? PairKey{ ColliderHandle(a), ColliderHandle(b) }
		: PairKey{ ColliderHandle(b), ColliderHandle(a) };
	}

	static bool IsCollidable(ColliderComponent* collider) {
		return collider->IsEnabled() && collider->GetOwner()->IsActive();
	}

	// --- 形状ごとの重なり判定 -------------------------------------------

	static bool Overlaps(const ColliderComponent& a, const ColliderComponent& b) {
		if (a.GetShape() == ColliderShape::Sphere && b.GetShape() == ColliderShape::Sphere) {
			return SphereVsSphere(a, b);
		}
		if (a.GetShape() == ColliderShape::AABB && b.GetShape() == ColliderShape::AABB) {
			return AABBVsAABB(a, b);
		}
		// 残りは必ずSphere-AABBの組み合わせになる
		const ColliderComponent& sphere = (a.GetShape() == ColliderShape::Sphere) ? a : b;
		const ColliderComponent& aabb = (a.GetShape() == ColliderShape::AABB) ? a : b;
		return SphereVsAABB(sphere, aabb);
	}

	static bool SphereVsSphere(const ColliderComponent& a, const ColliderComponent& b) {
		const Math::Vector3 diff = a.GetWorldCenter() - b.GetWorldCenter();
		const float radiusSum = a.GetRadius() + b.GetRadius();
		return diff.LengthSquared() <= radiusSum * radiusSum;
	}

	static bool AABBVsAABB(const ColliderComponent& a, const ColliderComponent& b) {
		const Math::Vector3 aCenter = a.GetWorldCenter();
		const Math::Vector3 bCenter = b.GetWorldCenter();
		const Math::Vector3& aHalf = a.GetHalfExtents();
		const Math::Vector3& bHalf = b.GetHalfExtents();

		return std::abs(aCenter.x - bCenter.x) <= (aHalf.x + bHalf.x)
			&& std::abs(aCenter.y - bCenter.y) <= (aHalf.y + bHalf.y)
			&& std::abs(aCenter.z - bCenter.z) <= (aHalf.z + bHalf.z);
	}

	static bool SphereVsAABB(const ColliderComponent& sphere, const ColliderComponent& aabb) {
		const Math::Vector3 sphereCenter = sphere.GetWorldCenter();
		const Math::Vector3 boxCenter = aabb.GetWorldCenter();
		const Math::Vector3& half = aabb.GetHalfExtents();

		// AABB内でsphereCenterに最も近い点を求め、その点との距離で判定する
		const Math::Vector3 closest{
			std::clamp(sphereCenter.x, boxCenter.x - half.x, boxCenter.x + half.x),
			std::clamp(sphereCenter.y, boxCenter.y - half.y, boxCenter.y + half.y),
			std::clamp(sphereCenter.z, boxCenter.z - half.z, boxCenter.z + half.z),
		};

		const Math::Vector3 diff = sphereCenter - closest;
		return diff.LengthSquared() <= sphere.GetRadius() * sphere.GetRadius();
	}

	// --- イベント通知 -------------------------------------------------

	// CollisionEnterEvent/CollisionExitEventはEventを継承しているため、
	// 集成体初期化 { a, b, c, d } をそのまま使うのは避ける。
	// C++17の集成体初期化では基底クラスも先頭の1要素として数えられ、
	// 意図せず最初の初期化子が(メンバではなく)Event基底部分に
	// 食われてしまう事故を招きやすいため、明示的なフィールド代入で組み立てる。
	static CollisionEnterEvent MakeEnterEvent(ColliderComponent* self, ColliderComponent* other) {
		CollisionEnterEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		return e;
	}

	static CollisionExitEvent MakeExitEvent(ColliderComponent* self, ColliderComponent* other) {
		CollisionExitEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		return e;
	}

	static void PublishEnter(ColliderComponent* a, ColliderComponent* b) {
		EventBus* bus = GetEventBus(a);
		if (bus == nullptr) return;

		// 片方ずつの視点でイベントを発行する。購読側は自分のGameObjectが
		// selfObjectと一致するものだけを見ればよい。
		bus->Publish(MakeEnterEvent(a, b));
		bus->Publish(MakeEnterEvent(b, a));
	}

	static void PublishExit(ColliderComponent* a, ColliderComponent* b) {
		EventBus* bus = GetEventBus(a);
		if (bus == nullptr) return;

		bus->Publish(MakeExitEvent(a, b));
		bus->Publish(MakeExitEvent(b, a));
	}

	static EventBus* GetEventBus(ColliderComponent* collider) {
		if (SceneContext* ctx = collider->GetOwner()->GetContext()) {
			return ctx->eventBus;
		}
		return nullptr;
	}

	// 前フレーム時点で重なっていたペアの集合。Enter/Exitの判定に使う。
	std::unordered_set<PairKey, PairKeyHash> previousOverlaps_;
};