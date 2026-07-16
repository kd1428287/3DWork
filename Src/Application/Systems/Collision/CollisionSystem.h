#pragma once

#include "CollisionMath.h"
#include "ColliderRegistry.h"

// ============================================================
// 当たり判定システム。
//
// InputSystem/CameraSystemと同じ「シーンに1つ、外側から明示的に
// Update()を呼んでもらう」System。対象コライダーは固定登録ではなく、
// ColliderRegistryが1フレームに1回キャッシュした一覧を使う
// (RaycastSystemと共有することで、同じ内容の再スキャンを避けている。
//  詳細はColliderRegistry参照)。
//
// ColliderComponentが複数の名前付き形状を持てるようになったため、
// ペアの総当たりは「コンポーネント×コンポーネント」ではなく
// 「形状×形状」の粒度で行う。重なり判定の実際の幾何計算は
// CollisionMath(状態を持たない純粋関数群)に委譲し、このクラスは
// 「誰と誰の、どの形状同士が、前フレームから状態が変わったか」の
// 記録とイベント発行だけに専念する。
//
// 今回のスコープ(意図的に簡略化している点。改良時の見取り図として
// 明記しておく):
//   - ブロードフェーズ無し。全ペアを総当たりでチェックする(O(n^2))。
//     コライダー数が増えたら空間分割(グリッド/BVH等)の導入を検討する。
//   - 押し返し(位置補正)は実装済み(ResolvePushBack参照)だが簡易版。
//     質量を考慮しない(静的/非静的の二値のみ)、複数接触の同時解決は
//     しない(ペアごとの逐次補正)、回転には応答しない、という制約がある。
//   - 連続衝突検出(トンネリング対策)は無い。1フレームで貫通するほど
//     速いオブジェクトがあれば当たり判定をすり抜ける。
//   - Sphere-Sphere / Sphere-AABB / AABB-AABB の3パターンのみ対応。
//   - 重なりペアの記録にはComponentHandle<ColliderComponent>+形状名を
//     使っている。生ポインタのままだと、破棄されたコンポーネントの
//     ヒープ領域が同じフレーム内で別の新しいコンポーネントに再利用
//     された場合、「まだ生きているポインタに見えるが実は別物」という
//     事故(ABA問題)を招く。世代番号付きのハンドルで、そのすり替わりを
//     確実に検出する。形状は名前で引き直す(vector再確保で生ポインタが
//     invalidateされるのを避けるため、インデックスではなく名前で参照する)。
//
// 再入(reentrancy)への配慮:
//   Update()は「検出(Phase 1)」と「通知(Phase 2)」を完全に分離している。
//   EventBus::Publish()は購読者のコールバックを同期実行するため、もし
//   検出中(a->GetShapes()/b->GetShapes()をライブに参照している最中)に
//   Publish()を呼ぶと、コールバック側が「ヒットした瞬間にこの形状を
//   消す/追加する」というアクションゲームでは典型的な処理をしただけで、
//   今まさに参照中のvectorが書き換わりイテレータ破壊やダングリング
//   参照を引き起こす。検出フェーズでは一切Publish()を呼ばず、全ペアの
//   判定が終わった後にまとめて通知することで、この問題を構造的に防ぐ。
//   なお、Publish()のコールバック内でGameObject/コンポーネントを
//   破棄したい場合は、GameObject::RequestRemoveComponent()等の
//   予約系APIを使うこと(即時破棄はこのフレーム中の他の処理から
//   見てダングリングポインタを生む可能性があるのは、他のシステムと
//   同様にこのシステムでも変わらない)。
// ============================================================
class CollisionSystem {
public:
	// Events::Collision::～ と毎回書かずに済むよう、クラス内だけに限定した
	// using宣言(ヘッダ全体やincludeした側の名前空間を汚さないため)。
	using CollisionEnterEvent = Events::Collision::CollisionEnterEvent;
	using CollisionExitEvent = Events::Collision::CollisionExitEvent;

	// ColliderRegistryが1フレームに1回キャッシュした一覧を使い、形状単位で
	// 総当たりの重なりをチェックする。Scene側の「このフレームの移動は
	// 全て確定した」タイミング(ObjectManager::Update()の後、Flush()の前
	// あたり)で、ColliderRegistry::Refresh()より後に呼ぶことを想定している。
	// 検出/通知の分離については、クラス冒頭のコメント「再入への配慮」を参照。
	void Update(const ColliderRegistry& registry) {
		const std::vector<ColliderComponent*>& colliders = registry.GetColliders();

		std::unordered_set<PairKey, PairKeyHash> currentOverlaps;
		std::vector<std::pair<PairKey, CollisionMath::OverlapResult>> newlyEntered;

		// --- Phase 1: 検出のみ。ユーザーコードは一切呼ばない -------------
		for (size_t i = 0; i < colliders.size(); ++i) {
			ColliderComponent* a = colliders[i];
			if (!IsCollidable(a)) continue;

			for (size_t j = i + 1; j < colliders.size(); ++j) {
				ColliderComponent* b = colliders[j];
				if (!IsCollidable(b)) continue;

				// 同じGameObjectに付いている場合、自分同士は判定しない
				if (a->GetOwner() == b->GetOwner()) continue;

				for (const CollisionShapeEntry& shapeA : a->GetShapes()) {
					if (!shapeA.enabled) continue;

					for (const CollisionShapeEntry& shapeB : b->GetShapes()) {
						if (!shapeB.enabled) continue;
						if (!shapeA.CanCollideWith(shapeB)) continue;

						const CollisionMath::OverlapResult overlap = Overlaps(*a, shapeA, *b, shapeB);
						if (!overlap.hit) continue;

						// 押し返し(位置補正)。両方が非トリガーの時だけ行う。
						// イベントの発行(Enter/Exit)とは独立して、重なっている
						// 間は毎フレーム実行する必要があるため、状態変化の
						// 記録より前、検出したその場で直接位置を書き換える
						// (Translate()はshapes_を触らないため、この形状リストの
						// イテレーション自体を壊す心配はない)。
						if (!shapeA.isTrigger && !shapeB.isTrigger) {
							ResolvePushBack(a, shapeA, b, shapeB, overlap);
						}

						const PairKey key = MakeKey(a, shapeA, b, shapeB);
						currentOverlaps.insert(key);

						// 前フレームまで重なっていなければ「新しく重なった」= Enter
						if (previousOverlaps_.find(key) == previousOverlaps_.end()) {
							newlyEntered.emplace_back(key, overlap);
						}
					}
				}
			}
		}

		// 前フレームは重なっていたが今回は重なりが消えたペア = Exit
		std::vector<PairKey> exitedKeys;
		for (const PairKey& key : previousOverlaps_) {
			if (currentOverlaps.find(key) == currentOverlaps.end()) {
				exitedKeys.push_back(key);
			}
		}

		previousOverlaps_ = std::move(currentOverlaps);

		// --- Phase 2: 通知。検出が完全に終わった後にだけ発行する ---------
		// このループ自体はもう検出用のライブなvectorを参照していないので、
		// ハンドラがコライダーの構造を書き換えてもイテレータは壊れない。
		// ただし「同じPhase2内の、まだ発行していない後続のペア」がすでに
		// 無効化されている可能性はあるため、発行の直前で必ずハンドル+
		// 形状名から解決し直し、解決できなければ静かにスキップする
		// (ComponentHandle::Resolve()は世代番号まで検証するため、
		//  破棄後にポインタ値だけ再利用された別物を誤って生存扱いしない)。
		for (auto& entry : newlyEntered) {
			const PairKey& key = entry.first;
			const CollisionMath::OverlapResult& overlap = entry.second;

			ColliderComponent* a = key.first.collider.Resolve();
			ColliderComponent* b = key.second.collider.Resolve();
			if (a == nullptr || b == nullptr) continue;

			const CollisionShapeEntry* shapeA = a->FindShape(key.first.shapeName);
			const CollisionShapeEntry* shapeB = b->FindShape(key.second.shapeName);
			if (shapeA == nullptr || shapeB == nullptr) continue; // 発行前に消された

			PublishEnter(a, *shapeA, b, *shapeB, overlap);
		}

		for (const PairKey& key : exitedKeys) {
			ColliderComponent* a = key.first.collider.Resolve();
			ColliderComponent* b = key.second.collider.Resolve();
			if (a == nullptr || b == nullptr) continue;

			const CollisionShapeEntry* shapeA = a->FindShape(key.first.shapeName);
			const CollisionShapeEntry* shapeB = b->FindShape(key.second.shapeName);
			if (shapeA == nullptr || shapeB == nullptr) continue;

			PublishExit(a, *shapeA, b, *shapeB);
		}
	}

private:
	using ColliderHandle = ComponentHandle<ColliderComponent>;

	// 「どのコンポーネントの、どの名前の形状か」を指すキー。
	struct ShapeRef {
		ColliderHandle collider;
		std::string shapeName;

		bool operator==(const ShapeRef& other) const {
			return collider == other.collider && shapeName == other.shapeName;
		}
	};

	struct ShapeRefHash {
		size_t operator()(const ShapeRef& ref) const {
			const size_t h1 = std::hash<ColliderHandle>{}(ref.collider);
			const size_t h2 = std::hash<std::string>{}(ref.shapeName);
			return h1 ^ (h2 << 1);
		}
	};

	using PairKey = std::pair<ShapeRef, ShapeRef>;

	struct PairKeyHash {
		size_t operator()(const PairKey& key) const {
			const size_t h1 = ShapeRefHash{}(key.first);
			const size_t h2 = ShapeRefHash{}(key.second);
			return h1 ^ (h2 << 1);
		}
	};

	static PairKey MakeKey(
		ColliderComponent* colliderA, const CollisionShapeEntry& shapeA,
		ColliderComponent* colliderB, const CollisionShapeEntry& shapeB) {
		// 順序はコンポーネントのポインタ値で正規化する(A-B/B-Aで別キーに
		// ならないようにするため)。この比較は正規化用だけに使い、ハンドル
		// 自体の同一性比較には影響しない(世代番号もそれぞれ保持される)。
		ShapeRef refA{ ColliderHandle(colliderA), shapeA.name };
		ShapeRef refB{ ColliderHandle(colliderB), shapeB.name };
		return (colliderA < colliderB) ? PairKey{ refA, refB } : PairKey{ refB, refA };
	}

	static bool IsCollidable(ColliderComponent* collider) {
		return collider->IsEnabled() && collider->GetOwner()->IsActive();
	}

	// --- 形状ごとの重なり判定 -------------------------------------------

	static CollisionMath::OverlapResult Overlaps(
		const ColliderComponent& a, const CollisionShapeEntry& shapeA,
		const ColliderComponent& b, const CollisionShapeEntry& shapeB) {

		const Math::Vector3 centerA = a.GetShapeWorldCenter(shapeA);
		const Math::Vector3 centerB = b.GetShapeWorldCenter(shapeB);

		if (shapeA.shape == ColliderShape::Sphere && shapeB.shape == ColliderShape::Sphere) {
			return CollisionMath::SphereVsSphere(centerA, shapeA.radius, centerB, shapeB.radius);
		}
		if (shapeA.shape == ColliderShape::AABB && shapeB.shape == ColliderShape::AABB) {
			return CollisionMath::AABBVsAABB(centerA, shapeA.halfExtents, centerB, shapeB.halfExtents);
		}

		// 残りは必ずSphere-AABBの組み合わせになる。
		if (shapeA.shape == ColliderShape::Sphere) {
			return CollisionMath::SphereVsAABB(centerA, shapeA.radius, centerB, shapeB.halfExtents);
		}

		// shapeAがAABB、shapeBがSphereの場合。SphereVsAABBはsphere視点の
		// 法線を返すため、a視点(常に「aをbから押し出す向き」)に合わせて反転する。
		CollisionMath::OverlapResult result =
			CollisionMath::SphereVsAABB(centerB, shapeB.radius, centerA, shapeA.halfExtents);
		if (result.hit) {
			result.hitNormal = -result.hitNormal;
		}
		return result;
	}

	// --- 押し返し(位置補正) ---------------------------------------------
	// KdColliderには無かった機能。CollisionMathが計算済みの押し出し方向
	// (hitNormal)とめり込み量(overlapDistance)を使って、実際に位置を
	// 補正する。
	//
	// 簡易実装であることに注意:
	//   - 質量を考慮しない(静的/非静的の二値のみ)。
	//   - 同じフレーム内で複数の相手と同時に重なっている場合(角に挟まる等)、
	//     ペアごとに逐次補正するだけの簡易処理(Gauss-Seidel的な近似)。
	//     真の同時解決(全接触を連立して解く)はしていないため、
	//     多接触時に軽い震え(ジッター)が出る可能性がある。
	//   - 回転・トルクへの応答はない(並進移動のみ)。
	static void ResolvePushBack(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap) {

		if (overlap.overlapDistance <= 0.0f) return;
		if (shapeA.isStatic && shapeB.isStatic) return; // 両方静的なら押し返しようがない

		// overlap.hitNormalは「aをbから押し出す向き」
		if (shapeA.isStatic) {
			b->Translate(-overlap.hitNormal * overlap.overlapDistance);
		}
		else if (shapeB.isStatic) {
			a->Translate(overlap.hitNormal * overlap.overlapDistance);
		}
		else {
			const float half = overlap.overlapDistance * 0.5f;
			a->Translate(overlap.hitNormal * half);
			b->Translate(-overlap.hitNormal * half);
		}
	}

	// --- イベント通知 -------------------------------------------------

	static CollisionEnterEvent MakeEnterEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape,
		const CollisionMath::OverlapResult& hitResult, bool flipNormal) {

		CollisionEnterEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		e.hitResult = hitResult;
		if (flipNormal) {
			e.hitResult.hitNormal = -e.hitResult.hitNormal;
		}
		return e;
	}

	static CollisionExitEvent MakeExitEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape) {

		CollisionExitEvent e;
		e.selfObject = self->GetOwner();
		e.selfCollider = self;
		e.selfShapeName = selfShape.name;
		e.otherObject = other->GetOwner();
		e.otherCollider = other;
		e.otherShapeName = otherShape.name;
		return e;
	}

	static void PublishEnter(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap) {

		EventBus* bus = GetEventBus(a);
		if (bus == nullptr) return;

		// overlapのhitNormalは「aをbから押し出す向き」で計算されている。
		// a視点のイベントはそのまま、b視点のイベントは向きを反転させて使う。
		bus->Publish(MakeEnterEvent(a, shapeA, b, shapeB, overlap, false));
		bus->Publish(MakeEnterEvent(b, shapeB, a, shapeA, overlap, true));
	}

	static void PublishExit(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB) {

		EventBus* bus = GetEventBus(a);
		if (bus == nullptr) return;

		bus->Publish(MakeExitEvent(a, shapeA, b, shapeB));
		bus->Publish(MakeExitEvent(b, shapeB, a, shapeA));
	}

	static EventBus* GetEventBus(ColliderComponent* collider) {
		if (SceneContext* ctx = collider->GetOwner()->GetContext()) {
			return ctx->eventBus;
		}
		return nullptr;
	}

	// 前フレーム時点で重なっていた(コンポーネント,形状名)ペアの集合。
	// Enter/Exitの判定に使う。
	std::unordered_set<PairKey, PairKeyHash> previousOverlaps_;
};