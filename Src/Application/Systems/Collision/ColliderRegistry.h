#pragma once

#include "../../Components/Collision/ColliderComponent.h"
#include "../../Core/GameObject.h"

// ============================================================
// このフレームに存在する全ColliderComponentの一覧をキャッシュしておく。
//
// CollisionSystem::Update()とRaycastSystemは、どちらも
// 「今フレームの全ColliderComponent一覧」を必要とする。これを
// 呼び出しごとに毎回ObjectManager::FindComponents<ColliderComponent>()
// (全GameObjectを走査する)でスキャンし直すと、同じ内容を何度も
// 再収集することになり無駄が多い。特にRaycastSystemは「敵の視界判定」
// 「将来的な足IKの接地レイ」のように1フレームに何度も(キャラの数だけ)
// 呼ばれる想定のクエリのため、ここを共有できる効果が大きい。
//
// 使い方: Scene側が1フレームに1回だけRefresh()を呼ぶ
// (ObjectManager::Update()の後、CollisionSystem::Update()や
//  各種RaycastSystem呼び出しより前が想定タイミング)。
// 以降そのフレーム中は、CollisionSystem::Update()/RaycastSystemの
// 各呼び出しがこのキャッシュを共有して使う。
//
// 注意: Refresh()した後にAddComponent<ColliderComponent>()等で
// 新しいコライダーが増えても、次にRefresh()されるまでこのキャッシュには
// 反映されない。地形(Ground)のような、フレーム中に動的に増減しない
// コライダーであれば実害は無いが、攻撃判定のようにフレーム中に
// 動的にアタッチ/デタッチされうるコライダーをこのキャッシュ経由で
// 扱う場合は、同フレーム内には反映されない点に注意すること。
// ============================================================
class ColliderRegistry {
public:
	void Refresh(ObjectManager& objectManager) {
		colliders_ = objectManager.FindComponents<ColliderComponent>();
		objectManager.SetColliderRegistry(this);
	}

	const std::vector<ColliderComponent*>& GetColliders() const { return colliders_; }

private:
	std::vector<ColliderComponent*> colliders_;
};
