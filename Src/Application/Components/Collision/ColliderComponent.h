#pragma once

#include "../Transform/TransformComponent.h"
#include "../../Systems/Collision/CollisionMath.h"
#include "ColliderCategory.h"
#include "ColliderLayerMatrix.h"
#include "TriangleMeshData.h"

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
// --- 形状の種類 -----------------------------------------------------
// Sphere / Box(回転を考慮するOBB) / Capsule(線分+半径) /
// Mesh(KdModelCollision相当) / Polygon(KdPolygonCollision相当) の
// 5種類を、同じCollisionShapeEntryの
// リストの中に混在させて持てる。
//
// 以前はMesh/PolygonをMeshColliderComponent/PolygonColliderComponentという
// 専用コンポーネントに分離していたが、そのせいでCollisionSystem/
// RaycastSystem/ColliderRegistryが「ColliderComponent用」と
// 「TriangleColliderComponent用」の2系統のループ・キー・イベント型を
// 持つ羽目になり、実質同じロジックの重複が大きかった。GameObjectが
// 「型ごとに1インスタンス」というルールと、ColliderComponent自体が
// 元々「名前付きで複数形状を持てる」設計だったことを踏まえ、Mesh/Polygonも
// この形状リストの一種として統合した(Bullet PhysicsのbtCompoundShapeが
// 1つの剛体に球・箱・メッシュを混在させられるのに近い考え方)。
//
//   - Sphere: 球は回転に依存しないため、スケールはmax(scale.x,y,z)を
//     半径に掛けて近似する(不均一スケールを掛けると本来は楕円体に
//     なってしまうため、安全側(大きめ)に見積もる近似である点に注意)。
//   - Box: TransformComponentの回転・不均一スケールにそのまま追従する
//     OBBとして扱う(スケールはBOXのローカル軸ごとに独立にかかるため、
//     球と違い不均一スケールでも歪みは生じない)。
//   - Capsule: 線分(offset～capsuleEnd)+半径。キャラクターの主判定形状
//     として、段差や斜面に対してSphere/Boxより滑らかに追従する
//     (Unity CapsuleCollider / Unreal UCapsuleComponent相当)。半径の
//     スケール追従はSphereと同じ近似(max scaleを使う安全側の近似)。
//   - Mesh: shared_ptr<TriangleMeshData>(頂点/面配列+境界情報)を持つ。
//     同じモデルアセットを複数のGameObjectで共有できる。
//   - Polygon: 頂点配列(周回順、扇形三角形分割)をこのエントリ自身が
//     直接保持する。KdPolygonの頂点がインスタンス固有だったのと同じ考え方。
//
// Mesh/Polygon同士(どちらも三角形の集合)の当たり判定は非対応
// (地形同士がぶつかる状況は想定しないためで、Unity/PhysXなどでも
//  非凸メッシュ同士の衝突は基本的にサポートされないのと同じ理由)。
// ============================================================

enum class ColliderShape
{
	Sphere,
	Box,	// 回転を考慮するOBB。回転が無ければ結果的にAABBと同じ。
	Capsule,// 線分+半径。キャラクターの主判定形状として段差・斜面に強い
	// (Unity CapsuleCollider / Unreal UCapsuleComponent相当)。
	Mesh,	// KdModelCollision相当。shared_ptr<TriangleMeshData>を参照する。
	Polygon,// KdPolygonCollision相当。頂点配列をこのエントリ自身が持つ。
};

// Mesh/Polygonのように「三角形の集合」で表される形状かどうか。
// Sphere/Box(解析的な形状。閉じた式で判定できる)と、Mesh/Polygon
// (三角形をループして判定する)とで、CollisionSystem側の扱いが
// 分かれるために使う。
constexpr bool IsTriangleShape(ColliderShape shape)
{
	return shape == ColliderShape::Mesh || shape == ColliderShape::Polygon;
}

// 当たり判定の「用途」を表すカテゴリは ColliderCategory.h に分離した
// (以前はここに ColliderLayer という素のuint32_t定数の namespace が
//  あったが、enum class化 + 「複数カテゴリを合成できる」実態に合わせて
//  改名した。詳細はColliderCategory.h冒頭のコメント参照)。


// 1つの当たり判定形状。ColliderComponentが名前付きで複数個保持する。
struct CollisionShapeEntry
{
	std::string name;
	ColliderShape shape = ColliderShape::Sphere;

	// オーナーのTransformComponentから見たローカル座標系でのオフセット。
	// ワールド座標へは回転・スケールを含めて変換される
	// (ColliderComponent::GetShapeWorldCenter参照)。
	// Mesh/Polygonでは使わない(頂点データ自体がローカル座標系の
	// 原点を基準に置かれている前提のため)。
	Math::Vector3 offset{};

	// Sphereはradiusのみ、BoxはhalfExtentsのみ参照する
	float radius = 0.5f;
	Math::Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };

	// Capsule用。線分の始点はoffset(上のoffsetフィールドを流用)、
	// 終点はこちらを使う。どちらもローカル座標系(ワールドへは
	// offsetと同じくワールド行列で変換される)。半径はradiusを流用。
	Math::Vector3 capsuleEnd{};

	// Mesh用。同じアセットを複数のGameObjectで共有できる。
	std::shared_ptr<TriangleMeshData> meshData;

	// Polygon用。ローカル座標系、周回順、扇形三角形分割(0,i+1,i+2)。
	// ※重要: KdCollision.cppのPolygonsIntersectは(faceIdx,faceIdx+1,
	// faceIdx+2)という"スライド窓"分割だったため、KdSquarePolygonの
	// 頂点配列(三角形ストリップ順)をそのまま渡しても正しく分割されない。
	// MakeQuadVertices()はこの周回順で頂点を組み立てているので、
	// そちらを使う分には意識する必要はない。凸多角形が前提。
	std::vector<Math::Vector3> polygonVertices;

	// この形状が「何者であるか」。複数カテゴリを同時に名乗ってよい
	// (例: 坂道の地形はGround|Bumpを両方持ち、「上に乗れる」かつ
	//  「横から押しても動かない壁」の両方として扱われる)。
	// Box2Dのcategory相当。「layer」という以前の名前は「1つに属する」
	// という単数のニュアンスを連想させ実態と食い違っていたため改名した
	// (詳細はColliderCategory.h参照)。
	ColliderCategory categoryMask = ColliderCategory::Bump;

	// この形状が「誰と判定したいか」(相手のcategoryMaskとAND判定)。
	// Box2Dのmask相当。
	ColliderCategory collideMask = ColliderCategory::All;

	// お互いのcategoryMask/collideMaskの両方が噛み合っていれば判定対象と
	// する(片方だけが相手のマスクに一致していても、相手側が自分を無視
	// する設定なら判定しない、という双方向チェック)。
	bool CanCollideWith(const CollisionShapeEntry& other) const {
		return Any(collideMask & other.categoryMask)
			&& Any(other.collideMask & categoryMask);
	}

	// true: 押し返しなどの物理応答はせず、重なり検知(イベント)だけ行う。
	bool isTrigger = false;

	// true: 押し返しの対象から自分を除外する(地形・壁など、押されても
	// 動かないもの)。isTrigger=falseな形状同士が重なった時、
	// CollisionSystemが自動的に位置補正を行うが、isStatic=trueの側は
	// 動かさず、相手側だけを押し出す(詳細はCollisionSystem参照)。
	// Mesh/Polygonは地形などの「動かない側」として使うことが多いため、
	// AddMesh/AddPolygonはデフォルトでtrueにする(下記参照)。
	bool isStatic = false;

	bool enabled = true;

	// true: 重なっている間、CollisionEnterEvent相当の通知を毎フレーム
	// 追加で受け取る(Unity等のOnTriggerStayに相当)。CollisionSystemは
	// Enter/Exit(状態が変化した瞬間)しか通知しないため、継続効果
	// (継続ダメージ、エリア内での継続処理等)が必要な形状だけこれを
	// trueにする。Ground/Bumpのような「常に重なっていて当然」の形状は
	// falseのままにしておくことで、無駄なPublish()を避ける
	// (詳細はCollisionSystem::Update()のStay通知パス参照)。
	bool wantsStayEvent = false;

	// --- Mesh/Polygon用: ローカル三角形へのアクセス -----------------

	size_t GetLocalTriangleCount() const
	{
		if (shape == ColliderShape::Mesh) {
			return meshData ? meshData->triangles.size() : 0;
		}
		if (shape == ColliderShape::Polygon) {
			return (polygonVertices.size() >= 3) ? (polygonVertices.size() - 2) : 0;
		}
		return 0;
	}

	void GetLocalTriangle(size_t index, Math::Vector3& outV0, Math::Vector3& outV1, Math::Vector3& outV2) const
	{
		if (shape == ColliderShape::Mesh) {
			const std::array<uint32_t, 3>& tri = meshData->triangles[index];
			outV0 = meshData->positions[tri[0]];
			outV1 = meshData->positions[tri[1]];
			outV2 = meshData->positions[tri[2]];
			return;
		}
		// Polygon: 扇形三角形分割
		outV0 = polygonVertices[0];
		outV1 = polygonVertices[index + 1];
		outV2 = polygonVertices[index + 2];
	}

	// ローカル座標系での境界情報(ブロードフェーズ用)。
	// Meshは事前計算済みのTriangleMeshDataの値をそのまま使う。
	// Polygonは呼ばれるたびに頂点配列から計算する(数が少ないため
	// 都度計算でも問題ない。ワールドキャッシュが変わった時にしか
	// 呼ばれないため、毎フレームではない)。
	void GetLocalBounds(Math::Vector3& outCenter, float& outRadius) const
	{
		if (shape == ColliderShape::Mesh) {
			outCenter = meshData ? meshData->boundsCenter : Math::Vector3::Zero;
			outRadius = meshData ? meshData->boundsRadius : 0.0f;
			return;
		}

		if (polygonVertices.empty()) {
			outCenter = Math::Vector3::Zero;
			outRadius = 0.0f;
			return;
		}

		Math::Vector3 minP = polygonVertices[0];
		Math::Vector3 maxP = polygonVertices[0];
		for (const Math::Vector3& p : polygonVertices) {
			minP = Math::Vector3::Min(minP, p);
			maxP = Math::Vector3::Max(maxP, p);
		}
		outCenter = (maxP + minP) * 0.5f;

		outRadius = 0.0f;
		for (const Math::Vector3& p : polygonVertices) {
			outRadius = std::max(outRadius, (p - outCenter).Length());
		}
	}

	// --- Mesh/Polygon用: ワールド変換キャッシュ付きの判定本体 -------
	//
	// 以前TriangleColliderComponentが持っていたロジックをそのまま
	// この形状エントリに移した。「直前に使ったワールド行列と今回の
	// 行列が同じなら再変換をスキップする」キャッシュ方式のため、
	// 配置後に動かない地形(isStatic=true)は実質1回しか変換されない。
	// キャッシュはmutableなので、const参照のまま(CollisionSystem側は
	// const CollisionShapeEntry&を渡すだけ)呼び出せる。

	CollisionMath::OverlapResult TestTriangleVsSphere(
		const Math::Matrix& worldMatrix, const Math::Vector3& sphereCenter, float radius) const
	{
		RefreshTriangleWorldCacheIfNeeded(worldMatrix);

		CollisionMath::OverlapResult result;

		const CollisionMath::OverlapResult broad =
			CollisionMath::SphereVsSphere(sphereCenter, radius, cachedWorldBoundsCenter_, cachedWorldBoundsRadius_);
		if (!broad.hit) return result;

		Math::Vector3 pushedCenter = sphereCenter;
		Math::Vector3 lastHitPos;
		bool isHit = false;

		for (const auto& tri : cachedWorldTriangles_) {
			const CollisionMath::OverlapResult faceResult =
				CollisionMath::SphereVsTriangle(pushedCenter, radius, tri[0], tri[1], tri[2]);
			if (!faceResult.hit) continue;

			isHit = true;
			lastHitPos = faceResult.hitPos;
			pushedCenter += faceResult.hitNormal * faceResult.overlapDistance;
		}

		if (!isHit) return result;

		const Math::Vector3 totalPush = pushedCenter - sphereCenter;
		const float totalPushLen = totalPush.Length();

		result.hit = true;
		result.hitPos = lastHitPos;
		result.overlapDistance = totalPushLen;
		result.hitNormal = (totalPushLen > 1e-6f) ? (totalPush / totalPushLen) : Math::Vector3::Up;
		return result;
	}

	CollisionMath::OverlapResult TestTriangleVsOBB(
		const Math::Matrix& worldMatrix, const CollisionMath::OrientedBox& box) const
	{
		RefreshTriangleWorldCacheIfNeeded(worldMatrix);

		CollisionMath::OverlapResult result;

		const float boxBoundingRadius = box.halfExtents.Length();
		const CollisionMath::OverlapResult broad =
			CollisionMath::SphereVsSphere(box.center, boxBoundingRadius, cachedWorldBoundsCenter_, cachedWorldBoundsRadius_);
		if (!broad.hit) return result;

		CollisionMath::OrientedBox pushedBox = box;
		Math::Vector3 lastHitPos;
		bool isHit = false;

		for (const auto& tri : cachedWorldTriangles_) {
			const CollisionMath::OverlapResult faceResult =
				CollisionMath::OBBVsTriangle(pushedBox, tri[0], tri[1], tri[2]);
			if (!faceResult.hit) continue;

			isHit = true;
			lastHitPos = faceResult.hitPos;
			pushedBox.center += faceResult.hitNormal * faceResult.overlapDistance;
		}

		if (!isHit) return result;

		const Math::Vector3 totalPush = pushedBox.center - box.center;
		const float totalPushLen = totalPush.Length();

		result.hit = true;
		result.hitPos = lastHitPos;
		result.overlapDistance = totalPushLen;
		result.hitNormal = (totalPushLen > 1e-6f) ? (totalPush / totalPushLen) : Math::Vector3::Up;
		return result;
	}

	CollisionMath::OverlapResult TestTriangleVsCapsule(
		const Math::Matrix& worldMatrix, const CollisionMath::Capsule& capsule) const
	{
		RefreshTriangleWorldCacheIfNeeded(worldMatrix);

		CollisionMath::OverlapResult result;

		// ブロードフェーズ: カプセルの軸線分の中点を仮の中心として、
		// 「線分の半長+半径」を大まかな包含半径にした球で先に足切りする
		// (SphereVsSphereほど厳密ではないが、Meshの境界球との一発判定
		//  だけで十分に安く弾ける近似で問題ない)。
		const Math::Vector3 capsuleMid = (capsule.start + capsule.end) * 0.5f;
		const float capsuleBoundingRadius = (capsule.end - capsule.start).Length() * 0.5f + capsule.radius;
		const CollisionMath::OverlapResult broad =
			CollisionMath::SphereVsSphere(capsuleMid, capsuleBoundingRadius, cachedWorldBoundsCenter_, cachedWorldBoundsRadius_);
		if (!broad.hit) return result;

		CollisionMath::Capsule pushedCapsule = capsule;
		Math::Vector3 lastHitPos;
		bool isHit = false;

		for (const auto& tri : cachedWorldTriangles_) {
			const CollisionMath::OverlapResult faceResult =
				CollisionMath::CapsuleVsTriangle(pushedCapsule, tri[0], tri[1], tri[2]);
			if (!faceResult.hit) continue;

			isHit = true;
			lastHitPos = faceResult.hitPos;
			const Math::Vector3 push = faceResult.hitNormal * faceResult.overlapDistance;
			pushedCapsule.start += push;
			pushedCapsule.end += push;
		}

		if (!isHit) return result;

		const Math::Vector3 totalPush = pushedCapsule.start - capsule.start;
		const float totalPushLen = totalPush.Length();

		result.hit = true;
		result.hitPos = lastHitPos;
		result.overlapDistance = totalPushLen;
		result.hitNormal = (totalPushLen > 1e-6f) ? (totalPush / totalPushLen) : Math::Vector3::Up;
		return result;
	}

	CollisionMath::RayHitResult TestTriangleVsRay(
		const Math::Matrix& worldMatrix, const Math::Vector3& origin, const Math::Vector3& direction, float range) const
	{
		RefreshTriangleWorldCacheIfNeeded(worldMatrix);

		CollisionMath::RayHitResult result;

		const CollisionMath::RayHitResult broad =
			CollisionMath::RayVsSphere(origin, direction, range, cachedWorldBoundsCenter_, cachedWorldBoundsRadius_);
		if (!broad.hit) return result;

		float closestDist = range;
		bool isHit = false;

		for (const auto& tri : cachedWorldTriangles_) {
			const CollisionMath::RayHitResult faceResult =
				CollisionMath::RayVsTriangle(origin, direction, closestDist, tri[0], tri[1], tri[2]);
			if (!faceResult.hit) continue;

			isHit = true;
			closestDist = faceResult.distance;
			result = faceResult;
		}

		return isHit ? result : CollisionMath::RayHitResult{};
	}

private:
	void RefreshTriangleWorldCacheIfNeeded(const Math::Matrix& worldMatrix) const
	{
		if (hasCache_ && worldMatrix == cachedMatrix_) return;

		const size_t triCount = GetLocalTriangleCount();
		cachedWorldTriangles_.resize(triCount);
		for (size_t i = 0; i < triCount; ++i) {
			Math::Vector3 lv0, lv1, lv2;
			GetLocalTriangle(i, lv0, lv1, lv2);

			cachedWorldTriangles_[i][0] = Math::Vector3::Transform(lv0, worldMatrix);
			cachedWorldTriangles_[i][1] = Math::Vector3::Transform(lv1, worldMatrix);
			cachedWorldTriangles_[i][2] = Math::Vector3::Transform(lv2, worldMatrix);
		}

		Math::Vector3 localBoundsCenter;
		float localBoundsRadius;
		GetLocalBounds(localBoundsCenter, localBoundsRadius);

		Math::Vector3 scale;
		Math::Quaternion rotation;
		Math::Vector3 translation;
		// Matrix::Decompose()はDirectXTK(SimpleMath)側でconst修飾されて
		// いないメンバ関数のため、const Math::Matrix&のままでは呼び出せない。
		// (「戻り値を書き換えないのにconstではない」というライブラリ側の
		//  仕様なので、ここでは非constのコピーを作ってから呼ぶ)
		Math::Matrix worldMatrixMutable = worldMatrix;
		worldMatrixMutable.Decompose(scale, rotation, translation);
		const float maxScale = std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });

		cachedWorldBoundsCenter_ = Math::Vector3::Transform(localBoundsCenter, worldMatrix);
		cachedWorldBoundsRadius_ = localBoundsRadius * maxScale;

		cachedMatrix_ = worldMatrix;
		hasCache_ = true;
	}

	// Mesh/Polygon専用。Sphere/Boxのエントリでは未使用のまま(コストは
	// 空vector+行列1つ分程度で、Sphere/Box用途では無視できる)。
	mutable std::vector<std::array<Math::Vector3, 3>> cachedWorldTriangles_;
	mutable Math::Vector3	cachedWorldBoundsCenter_{};
	mutable float			cachedWorldBoundsRadius_ = 0.0f;
	mutable Math::Matrix	cachedMatrix_ = Math::Matrix::Identity;
	mutable bool			hasCache_ = false;
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
		const Math::Vector3& offset = {}, ColliderCategory category = ColliderCategory::Bump,
		ColliderCategory collideMask = ColliderCategory::UseMatrixDefault) {
		CollisionShapeEntry entry;
		entry.name = name;
		entry.shape = ColliderShape::Sphere;
		entry.radius = radius;
		entry.offset = offset;
		entry.categoryMask = category;
		entry.collideMask = ResolveCollideMask(category, collideMask);
		return AddOrReplace(std::move(entry));
	}

	// 回転を考慮するBOX(OBB)を登録する。回転させないオブジェクトに
	// 使う分には、これまでのAABBと同じ結果になる。
	CollisionShapeEntry& AddBox(std::string_view name, const Math::Vector3& halfExtents,
		const Math::Vector3& offset = {}, ColliderCategory category = ColliderCategory::Bump,
		ColliderCategory collideMask = ColliderCategory::UseMatrixDefault) {
		CollisionShapeEntry entry;
		entry.name = name;
		entry.shape = ColliderShape::Box;
		entry.halfExtents = halfExtents;
		entry.offset = offset;
		entry.categoryMask = category;
		entry.collideMask = ResolveCollideMask(category, collideMask);
		return AddOrReplace(std::move(entry));
	}

	// Capsule(線分+半径)を登録する。始点・終点をローカル座標で明示的に
	// 指定する版。キャラクター以外(横倒しの障害物など、上下方向以外に
	// 伸びるカプセル)を作りたい場合はこちらを使う。
	CollisionShapeEntry& AddCapsule(std::string_view name, float radius,
		const Math::Vector3& localStart, const Math::Vector3& localEnd,
		ColliderCategory category = ColliderCategory::Bump,
		ColliderCategory collideMask = ColliderCategory::UseMatrixDefault) {
		CollisionShapeEntry entry;
		entry.name = name;
		entry.shape = ColliderShape::Capsule;
		entry.radius = radius;
		entry.offset = localStart;
		entry.capsuleEnd = localEnd;
		entry.categoryMask = category;
		entry.collideMask = ResolveCollideMask(category, collideMask);
		return AddOrReplace(std::move(entry));
	}

	// Capsule登録の簡易版: キャラクターの主判定形状としての典型的な
	// 使い方(トランスフォーム原点を中心に、上下(+Y/-Y)方向へ伸びる
	// カプセル)を「全高height + 半径radius」だけで組み立てる。
	// 半球の分もheightに含む(全高 = 円柱部分の長さ + radius*2)ため、
	// 円柱部分の長さが負にならないようclampしている。
	CollisionShapeEntry& AddCapsule(std::string_view name, float height, float radius,
		ColliderCategory category = ColliderCategory::Bump,
		ColliderCategory collideMask = ColliderCategory::UseMatrixDefault) {
		const float cylinderHalfLength = std::max(0.0f, height * 0.5f - radius);
		const Math::Vector3 localStart(0.0f, -cylinderHalfLength, 0.0f);
		const Math::Vector3 localEnd(0.0f, cylinderHalfLength, 0.0f);
		return AddCapsule(name, radius, localStart, localEnd, category, collideMask);
	}

	// モデル単位の精密な当たり判定(KdModelCollision相当)を登録する。
	// 同じdataを複数のGameObjectで共有できる(TriangleMeshData参照)。
	// 地形などの動かない用途を想定し、isStatic/categoryはデフォルトで
	// 「地形向け」にしている(必要なら戻り値経由で変更可能)。
	CollisionShapeEntry& AddMesh(std::string_view name, const std::shared_ptr<TriangleMeshData>& data,
		ColliderCategory category = ColliderCategory::Ground,
		ColliderCategory collideMask = ColliderCategory::UseMatrixDefault) {
		CollisionShapeEntry entry;
		entry.name = name;
		entry.shape = ColliderShape::Mesh;
		entry.meshData = data;
		entry.categoryMask = category;
		entry.collideMask = ResolveCollideMask(category, collideMask);
		entry.isStatic = true;
		return AddOrReplace(std::move(entry));
	}

	// 単一の凸多角形単位の精密な当たり判定(KdPolygonCollision相当)を
	// 登録する。頂点は周回順、3点以上(SetQuad系のヘルパーはMakeQuadVertices参照)。
	CollisionShapeEntry& AddPolygon(std::string_view name, std::vector<Math::Vector3> localVertices,
		ColliderCategory category = ColliderCategory::Ground,
		ColliderCategory collideMask = ColliderCategory::UseMatrixDefault) {
		CollisionShapeEntry entry;
		entry.name = name;
		entry.shape = ColliderShape::Polygon;
		entry.polygonVertices = std::move(localVertices);
		entry.categoryMask = category;
		entry.collideMask = ResolveCollideMask(category, collideMask);
		entry.isStatic = true;
		return AddOrReplace(std::move(entry));
	}

	// KdSquarePolygonのInitVertices()相当の便利関数:
	// XY平面上の四角形(ローカル原点中心)の頂点を周回順で生成する。
	// AddPolygon()にそのまま渡せる。
	static std::vector<Math::Vector3> MakeQuadVertices(float width, float height) {
		const float halfX = width * 0.5f;
		const float halfY = height * 0.5f;
		return {
			Math::Vector3(-halfX, -halfY, 0.0f),
			Math::Vector3(halfX, -halfY, 0.0f),
			Math::Vector3(halfX,  halfY, 0.0f),
			Math::Vector3(-halfX,  halfY, 0.0f),
		};
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

	// true: 押し返しなどの物理応答をやめ、重なり検知(イベント)だけにする。
	// 攻撃判定(HitBox/HurtBox)のように、常に非トリガーで生成してから
	// 個別にトリガー化したいケースや、逆に地形風の形状を一時的に
	// トリガー化してすり抜け可能にしたい、といった用途向け。
	void SetShapeIsTrigger(std::string_view name, bool isTrigger) {
		if (CollisionShapeEntry* entry = FindShape(name)) {
			entry->isTrigger = isTrigger;
		}
	}

	// true: 押し返しの対象から自分を除外する(地形・壁など、押されても
	// 動かないものにする)。詳細はCollisionShapeEntry::isStaticのコメント参照。
	void SetShapeIsStatic(std::string_view name, bool isStatic) {
		if (CollisionShapeEntry* entry = FindShape(name)) {
			entry->isStatic = isStatic;
		}
	}

	// true: 重なっている間、CollisionEnterEvent相当の通知を毎フレーム
	// 追加で受け取る(Unity等のOnTriggerStayに相当)。詳細は
	// CollisionShapeEntry::wantsStayEventのコメント参照。
	void SetShapeWantsStayEvent(std::string_view name, bool wantsStayEvent) {
		if (CollisionShapeEntry* entry = FindShape(name)) {
			entry->wantsStayEvent = wantsStayEvent;
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

	// Mesh/Polygon形状のワールド変換に使う、オーナーのワールド行列。
	Math::Matrix GetWorldMatrix() const {
		return transform_ ? transform_->GetWorldMatrix() : Math::Matrix::Identity;
	}

	// 指定した形状のワールド座標での中心点。
	// TransformComponentのワールド行列(親の階層・回転・スケールを
	// 全て含む)でoffsetを変換するため、キャラクターが回転/移動すれば
	// オフセット付きの形状もそれに追従する。
	Math::Vector3 GetShapeWorldCenter(const CollisionShapeEntry& entry) const {
		if (transform_ == nullptr) { return entry.offset; }
		return Math::Vector3::Transform(entry.offset, transform_->GetWorldMatrix());
	}

	// Sphere形状のワールド半径。不均一スケールは球の形を保てないため、
	// max(scale.x,scale.y,scale.z)を掛けた近似値を返す
	// (実際より小さく見積もって当たり判定が抜けるより、安全側に
	//  大きめに見積もる方針)。
	float GetShapeWorldRadius(const CollisionShapeEntry& entry) const {
		if (transform_ == nullptr) { return entry.radius; }

		Math::Vector3 scale;
		Math::Quaternion rotation;
		Math::Vector3 translation;
		transform_->GetWorldMatrix().Decompose(scale, rotation, translation);

		const float maxScale = std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
		return entry.radius * maxScale;
	}

	// Box形状のワールド空間でのOBB(中心・半径サイズ・向き)。
	// halfExtentsにはTransformComponentのスケールをそのまま(軸ごとに
	// 独立に)掛ける。BOXは回転する形状なので、球と違い不均一スケールを
	// 掛けても歪まず、そのまま有効なOBBになる。
	CollisionMath::OrientedBox GetShapeWorldOBB(const CollisionShapeEntry& entry) const {
		CollisionMath::OrientedBox box;
		box.halfExtents = entry.halfExtents;

		if (transform_ == nullptr) {
			box.center = entry.offset;
			return box;
		}

		Math::Vector3 scale;
		Math::Quaternion rotation;
		Math::Vector3 translation;
		transform_->GetWorldMatrix().Decompose(scale, rotation, translation);

		box.center = Math::Vector3::Transform(entry.offset, transform_->GetWorldMatrix());
		box.orientation = rotation;
		box.halfExtents = Math::Vector3(
			entry.halfExtents.x * std::abs(scale.x),
			entry.halfExtents.y * std::abs(scale.y),
			entry.halfExtents.z * std::abs(scale.z));
		return box;
	}

	// Capsule形状のワールド空間での線分+半径。
	// 半径はSphereと同じ理由でmax(scale.x,y,z)を掛ける近似
	// (不均一スケールをかけるとカプセルも本来は歪んだ形になるが、
	//  安全側に大きめで近似する。キャラクター用途では通常スケールは
	//  均一なので、実用上問題になるケースは少ない)。
	// 線分の両端(offset/capsuleEnd)はワールド行列でそのまま変換する
	// (回転・平行移動・不均一スケールを含めて追従させる。線分の"向き"
	//  自体は不均一スケールでも歪まないため、半径だけ近似すれば十分)。
	CollisionMath::Capsule GetShapeWorldCapsule(const CollisionShapeEntry& entry) const {
		CollisionMath::Capsule capsule;
		capsule.radius = GetShapeWorldRadius(entry);

		if (transform_ == nullptr) {
			capsule.start = entry.offset;
			capsule.end = entry.capsuleEnd;
			return capsule;
		}

		const Math::Matrix& worldMatrix = transform_->GetWorldMatrix();
		capsule.start = Math::Vector3::Transform(entry.offset, worldMatrix);
		capsule.end = Math::Vector3::Transform(entry.capsuleEnd, worldMatrix);
		return capsule;
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

	// --- ペア単位の判定除外 ---------------------------------------------
	//
	// categoryMask/collideMaskは「カテゴリ単位」の粗い制御しかできない
	// (例: 全てのHitBoxが全てのHurtBoxと判定される)。「同じカテゴリだが
	// この特定の相手とだけは判定したくない」というケース(自分が発射した
	// 弾は発射者自身には当たらない、等)のための個別除外機構。
	// Unity(Physics.IgnoreCollision)/Box2D(b2Filter.groupIndex)にある
	// 仕組みと同じ役割。
	//
	// GameObject単位の粒度にしている(形状名までは見ない)。同じ相手との
	// 除外を複数の形状にまたがって毎回書くのは煩雑になりやすく、実際に
	// 「この形状だけ当てたいがこの形状は無視したい」という細かい要求は
	// 今のところ想定していないため。将来必要になったら
	// (GameObject*, shapeName)のペアをキーにする形へ拡張すればよい。
	void IgnoreCollisionWith(GameObject* other) { ignoreList_.insert(other); }
	void StopIgnoringCollisionWith(GameObject* other) { ignoreList_.erase(other); }
	bool IsIgnoring(GameObject* other) const { return ignoreList_.count(other) > 0; }

private:
	CollisionShapeEntry& AddOrReplace(CollisionShapeEntry&& entry) {
		if (CollisionShapeEntry* existing = FindShape(entry.name)) {
			*existing = std::move(entry);
			return *existing;
		}
		shapes_.push_back(std::move(entry));
		return shapes_.back();
	}

	// collideMaskが省略時の目印(UseMatrixDefault)のままなら
	// ColliderLayerMatrixから解決し、明示的に指定されていればそのまま使う。
	static ColliderCategory ResolveCollideMask(ColliderCategory category, ColliderCategory collideMask) {
		if (collideMask == ColliderCategory::UseMatrixDefault) {
			return ColliderLayerMatrix::Instance().GetDefaultMask(category);
		}
		return collideMask;
	}

	TransformComponent* transform_ = nullptr;
	std::vector<CollisionShapeEntry> shapes_;

	// IsIgnoring()は毎フレームCollisionSystem::Update()のペアループから
	// 呼ばれるため、std::vectorの線形探索ではなくunordered_setにして
	// O(1)で引けるようにしている。
	std::unordered_set<GameObject*> ignoreList_;
};