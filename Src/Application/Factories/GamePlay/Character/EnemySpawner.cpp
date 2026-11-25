#include "EnemySpawner.h"
#include <cassert>

GameObject* EnemySpawner::Spawn(const std::string& enemyId, const Math::Vector3& position)
{
	if (!catalog_.Contains(enemyId)) {
		assert(false && "unknown enemyId");
		return nullptr;
	}
	GameObject* enemy = catalog_.Spawn(objectManager_, enemyId, position);
	// TODO: ターゲット設定など、生成後の初期化を行う
	return enemy;
}

void EnemySpawner::Despawn(GameObject* enemy)
{
	if (enemy == nullptr) return;
	// TODO: ObjectManagerの削除処理を呼ぶ
}
