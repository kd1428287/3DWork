#pragma once

#include "../../Core/Handle.h"
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
// Sphere/Box(OBB)/Mesh/Polygonの4種類の形状は、すべてColliderComponent
// のCollisionShapeEntryとして同じリストに混在する(以前はMesh/Polygonを
// MeshColliderComponent/PolygonColliderComponentという専用コンポーネントに
// 分離していたが、CollisionSystem/RaycastSystem/ColliderRegistryが
// 「ColliderComponent用」「TriangleColliderComponent用」の2系統を持つ
// 羽目になり、実質同じロジックの重複が大きかったため統合した)。
// これにより、このクラスのペア総当たりは1本のループ・1種類のキー・
// 1種類のイベントで完結する。
//
// Box形状は回転を考慮するOBBとして扱う(3Dアクションでの利用を前提に、
// TransformComponentの回転・不均一スケールにきちんと追従する。
// 詳細はColliderComponent::GetShapeWorldOBB/CollisionMath::OrientedBox
// 参照)。
//
// Mesh/Polygon同士(どちらも三角形の集合)の当たり判定は非対応
// (Overlaps()内でIsTriangleShape(shapeA.shape) && IsTriangleShape(shapeB.shape)
// のペアは無条件でノーヒット扱いにしている。地形同士がぶつかる状況は
// 想定しないためで、Unity/PhysXなどでも非凸メッシュ同士の衝突は基本的に
// サポートされないのと同じ理由)。
//
// 今回のスコープ(意図的に簡略化している点。改良時の見取り図として
// 明記しておく):
//   - ブロードフェーズ無し。全ペアを総当たりでチェックする(O(n^2))。
//     コライダー数が増えたら空間分割(グリッド/BVH等)の導入を検討する。
//     (Mesh/Polygon形状は各形状エントリ自身が持つ境界球による足切りは
//      あるが、コライダー一覧全体に対する空間分割ではない)
//   - 押し返し(位置補正)は実装済み(ResolvePushBack参照)だが簡易版。
//     質量を考慮しない(静的/非静的の二値のみ)、複数接触の同時解決は
//     しない(ペアごとの逐次補正)、トルク(回転方向の応答)は無く並進
//     移動のみ、という制約がある。
//   - 連続衝突検出(トンネリング対策)は無い。1フレームで貫通するほど
//     速いオブジェクトがあれば当たり判定をすり抜ける。
//   - 重なりペアの記録にはHandle<ColliderComponent>(Handle.h参照)+
//     形状名を使っている。生ポインタのままだと、破棄されたコンポーネント
//     のヒープ領域が同じフレーム内で別の新しいコンポーネントに再利用
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
//
// EventBus::Subscribe<T>/Publish<T>はTをEvent派生型として
// static_cast/暗黙アップキャストするため(EventBus.h参照)、
// 新しいイベント型を追加する際は必ずEventを継承すること。
// ============================================================
class CollisionSystem {
public:
	// Events::Collision::～ と毎回書かずに済むよう、クラス内だけに限定した
	// using宣言(ヘッダ全体やincludeした側の名前空間を汚さないため)。
	using CollisionEnterEvent = Events::Collision::CollisionEnterEvent;
	using CollisionExitEvent = Events::Collision::CollisionExitEvent;
	// CollisionEnterEventと同じ形のイベント。継続中の重なりを毎フレーム
	// 通知する(Unity等のOnTriggerStayに相当)。Events::Collision名前空間
	// 側にCollisionEnterEventと同じフィールド構成で追加しておくこと
	// (selfObject/selfCollider/selfShapeName/otherObject/otherCollider/
	//  otherShapeName/hitResult)。Enterと型を分けているのは、購読側が
	// Subscribe<CollisionEnterEvent>とSubscribe<CollisionStayEvent>を
	// 独立に選べるようにするため(型を共有すると、Enterだけ欲しい購読者に
	// Stayまで混ざって届いてしまう)。
	using CollisionStayEvent = Events::Collision::CollisionStayEvent;

	// ColliderRegistryが1フレームに1回キャッシュした一覧を使い、形状単位で
	// 総当たりの重なりをチェックする。Scene側の「このフレームの移動は
	// 全て確定した」タイミング(ObjectManager::Update()の後、Flush()の前
	// あたり)で、ColliderRegistry::Refresh()より後に呼ぶことを想定している。
	// 検出/通知の分離については、クラス冒頭のコメント「再入への配慮」を参照。
	void Update(const ColliderRegistry& registry) {
		const std::vector<ColliderComponent*>& colliders = registry.GetColliders();

		// keyだけでなくOverlapResultも一緒に持たせておく。理由は2つ:
		// ・Exit判定のためには前フレームとの差分(キーの集合)だけで十分だが、
		//   Stay通知(このフレームも重なり続けているペア)を出すには
		//   overlap自体(hitPos/hitNormal等)が要る。Phase1で既に計算済みの
		//   結果を使い回すことで、Stay用にOverlaps()を二重に呼ばずに済む。
		std::unordered_map<PairKey, CollisionMath::OverlapResult, PairKeyHash> currentOverlaps;
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
						currentOverlaps.emplace(key, overlap);

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
		for (const auto& prevEntry : previousOverlaps_) {
			const PairKey& key = prevEntry.first;
			if (currentOverlaps.find(key) == currentOverlaps.end()) {
				exitedKeys.push_back(key);
			}
		}

		// 前フレームから継続して重なっているペア = Stay
		// (このフレームで新規にEnterしたペアはここでは除外する。
		//  Enterイベントと同じフレームでStayも同時に飛ばすかどうかは
		//  流儀があるが、ここでは「Enterの次のフレームからStay」という
		//  分かりやすい方を採用している)。
		std::vector<std::pair<PairKey, CollisionMath::OverlapResult>> staying;
		for (const auto& entry : currentOverlaps) {
			const PairKey& key = entry.first;
			if (previousOverlaps_.find(key) == previousOverlaps_.end()) continue; // 今回Enterした分は除外
			staying.emplace_back(entry);
		}

		previousOverlaps_ = std::move(currentOverlaps);

		// --- Phase 2: 通知。検出が完全に終わった後にだけ発行する ---------
		// このループ自体はもう検出用のライブなvectorを参照していないので、
		// ハンドラがコライダーの構造を書き換えてもイテレータは壊れない。
		// ただし「同じPhase2内の、まだ発行していない後続のペア」がすでに
		// 無効化されている可能性はあるため、発行の直前で必ずハンドル+
		// 形状名から解決し直し、解決できなければ静かにスキップする
		// (Handle::Resolve()は世代番号まで検証するため、破棄後にポインタ値
		//  だけ再利用された別物を誤って生存扱いしない)。
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

		// --- Phase 2.5: Stay通知。継続中のペアのうち、どちらかの形状が
		// wantsStayEvent=trueの場合だけ発行する。Ground/Bumpのような
		// 「常に重なっていて当然」の形状はデフォルトfalseなので、大半の
		// ペアはここでほぼコスト無く弾かれる(overlap自体はPhase1で
		// 計算済みのものを使い回すため、再判定は発生しない)。
		for (auto& entry : staying) {
			const PairKey& key = entry.first;
			const CollisionMath::OverlapResult& overlap = entry.second;

			ColliderComponent* a = key.first.collider.Resolve();
			ColliderComponent* b = key.second.collider.Resolve();
			if (a == nullptr || b == nullptr) continue;

			const CollisionShapeEntry* shapeA = a->FindShape(key.first.shapeName);
			const CollisionShapeEntry* shapeB = b->FindShape(key.second.shapeName);
			if (shapeA == nullptr || shapeB == nullptr) continue;

			if (!shapeA->wantsStayEvent && !shapeB->wantsStayEvent) continue;

			PublishStay(a, *shapeA, b, *shapeB, overlap, shapeA->wantsStayEvent, shapeB->wantsStayEvent);
		}
	}

private:
	using ColliderHandle = Handle<ColliderComponent>;

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
	// 戻り値のhitNormalは常に「shapeAをshapeBから押し出す向き」に揃える。

	static CollisionMath::OverlapResult Overlaps(
		ColliderComponent& a, const CollisionShapeEntry& shapeA,
		ColliderComponent& b, const CollisionShapeEntry& shapeB) {

		const bool aIsTri = IsTriangleShape(shapeA.shape);
		const bool bIsTri = IsTriangleShape(shapeB.shape);

		// Mesh/Polygon同士は非対応(クラス冒頭コメント参照)。
		if (aIsTri && bIsTri) {
			return CollisionMath::OverlapResult{};
		}

		if (aIsTri) {
			// shapeAがMesh/Polygon側。TestTriangleShapeは
			// 「otherShapeをtriShapeから押し出す向き」= 「shapeBをshapeAから
			// 押し出す向き」を返すため、Overlaps()の契約(shapeAをshapeBから
			// 押し出す向き)に合わせて反転する。
			CollisionMath::OverlapResult result = TestTriangleShape(a, shapeA, b, shapeB);
			if (result.hit) {
				result.hitNormal = -result.hitNormal;
			}
			return result;
		}
		if (bIsTri) {
			// TestTriangleShape(b, shapeB, a, shapeA)は「shapeAをshapeBから
			// 押し出す向き」をそのまま返すので反転不要。
			return TestTriangleShape(b, shapeB, a, shapeA);
		}

		// ここから先はSphere/Box(OBB)同士の組み合わせのみ。
		if (shapeA.shape == ColliderShape::Sphere && shapeB.shape == ColliderShape::Sphere) {
			const Math::Vector3 centerA = a.GetShapeWorldCenter(shapeA);
			const Math::Vector3 centerB = b.GetShapeWorldCenter(shapeB);
			return CollisionMath::SphereVsSphere(
				centerA, a.GetShapeWorldRadius(shapeA), centerB, b.GetShapeWorldRadius(shapeB));
		}

		if (shapeA.shape == ColliderShape::Box && shapeB.shape == ColliderShape::Box) {
			return CollisionMath::OBBVsOBB(a.GetShapeWorldOBB(shapeA), b.GetShapeWorldOBB(shapeB));
		}

		// 残りは必ずSphere-Box(OBB)の組み合わせになる。
		if (shapeA.shape == ColliderShape::Sphere) {
			const Math::Vector3 centerA = a.GetShapeWorldCenter(shapeA);
			return CollisionMath::SphereVsOBB(centerA, a.GetShapeWorldRadius(shapeA), b.GetShapeWorldOBB(shapeB));
		}

		// shapeAがBox、shapeBがSphereの場合。SphereVsOBBはsphere視点の
		// 法線を返すため、a視点(常に「aをbから押し出す向き」)に合わせて反転する。
		const Math::Vector3 centerB = b.GetShapeWorldCenter(shapeB);
		CollisionMath::OverlapResult result =
			CollisionMath::SphereVsOBB(centerB, b.GetShapeWorldRadius(shapeB), a.GetShapeWorldOBB(shapeA));
		if (result.hit) {
			result.hitNormal = -result.hitNormal;
		}
		return result;
	}

	// triOwner/triShapeがMesh/Polygon側。実際の反復判定は
	// CollisionShapeEntry::TestTriangleVsSphere/TestTriangleVsOBBに委譲する
	// (KdModelCollision/KdPolygonCollisionが自分の全ての面をループして
	// 結果を合成していたのと同じ責務分担)。
	// 戻り値のhitNormalは「otherShapeをtriShapeから押し出す向き」。
	static CollisionMath::OverlapResult TestTriangleShape(
		ColliderComponent& triOwner, const CollisionShapeEntry& triShape,
		ColliderComponent& other, const CollisionShapeEntry& otherShape) {

		const Math::Matrix triWorldMatrix = triOwner.GetWorldMatrix();

		if (otherShape.shape == ColliderShape::Sphere) {
			const Math::Vector3 center = other.GetShapeWorldCenter(otherShape);
			return triShape.TestTriangleVsSphere(triWorldMatrix, center, other.GetShapeWorldRadius(otherShape));
		}

		return triShape.TestTriangleVsOBB(triWorldMatrix, other.GetShapeWorldOBB(otherShape));
	}

	// --- 押し返し(位置補正) ---------------------------------------------
	// KdColliderには無かった機能。CollisionMathが計算済みの押し出し方向
	// (hitNormal)とめり込み量(overlapDistance)を使って、実際に位置を
	// 補正する。Sphere/Box/Mesh/Polygonのどの組み合わせでも、
	// isStatic/isTriggerはCollisionShapeEntryが共通で持っているため、
	// この関数1つで全パターンをまかなえる。
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

	static CollisionStayEvent MakeStayEvent(
		ColliderComponent* self, const CollisionShapeEntry& selfShape,
		ColliderComponent* other, const CollisionShapeEntry& otherShape,
		const CollisionMath::OverlapResult& hitResult, bool flipNormal) {

		// フィールド構成はCollisionEnterEventと同一。型を分けているのは
		// 購読側がEnter/Stayを別々に選べるようにするため(理由は
		// using CollisionStayEvent = ...の注釈を参照)。
		CollisionStayEvent e;
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

	// 宛先(self)のGameObjectが持つローカルバスにだけ発行する。
	// 以前はシーン共有のEventBus(SceneContext::eventBus)に投げていたため、
	// CollisionEnterEvent等を購読している「シーン内の無関係な全オブジェクト」
	// のハンドラまで毎回呼ばれ、各自でselfObject==自分かのフィルタが
	// 必要だった。宛先はPublish時点で確定しているので、最初から
	// self->GetOwner()->GetLocalEventBus()に直接投げることで、
	// 無関係なオブジェクトには物理的に届かなくなる
	// (GameObject::GetLocalEventBus()の追加が必要。無ければ
	//  GameObject.h側に「このGameObject宛て専用のEventBusインスタンス」
	//  を1つ持たせて公開するだけでよい)。
	static void PublishToOwner(ColliderComponent* self, auto&& event) {
		self->GetOwner()->GetLocalEventBus().Publish(std::forward<decltype(event)>(event));
	}

	static void PublishEnter(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap) {

		// overlapのhitNormalは「aをbから押し出す向き」で計算されている。
		// a視点のイベントはそのまま、b視点のイベントは向きを反転させて使う。
		// それぞれ自分自身(self)のローカルバスにだけ発行する。
		PublishToOwner(a, MakeEnterEvent(a, shapeA, b, shapeB, overlap, false));
		PublishToOwner(b, MakeEnterEvent(b, shapeB, a, shapeA, overlap, true));
	}

	static void PublishExit(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB) {

		PublishToOwner(a, MakeExitEvent(a, shapeA, b, shapeB));
		PublishToOwner(b, MakeExitEvent(b, shapeB, a, shapeA));
	}

	// wantsStayA/wantsStayBは呼び出し側(Update())が既にチェック済みだが、
	// 「shapeAだけ欲しい」「shapeBだけ欲しい」というケースもあるため、
	// 片側だけ発行することを許すために個別に渡す。
	static void PublishStay(
		ColliderComponent* a, const CollisionShapeEntry& shapeA,
		ColliderComponent* b, const CollisionShapeEntry& shapeB,
		const CollisionMath::OverlapResult& overlap,
		bool wantsStayA, bool wantsStayB) {

		if (wantsStayA) {
			PublishToOwner(a, MakeStayEvent(a, shapeA, b, shapeB, overlap, false));
		}
		if (wantsStayB) {
			PublishToOwner(b, MakeStayEvent(b, shapeB, a, shapeA, overlap, true));
		}
	}

	// 前フレーム時点で重なっていた(コンポーネント,形状名)ペアの集合。
	// Enter/Exit/Stayの判定に使う。OverlapResultも一緒に保持しているのは
	// Stay通知のためで、Exit判定自体はキーの有無だけで足りる。
	std::unordered_map<PairKey, CollisionMath::OverlapResult, PairKeyHash> previousOverlaps_;
};
