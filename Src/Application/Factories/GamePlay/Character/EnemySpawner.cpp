#include "EnemySpawner.h"
#include <cassert>
#include "Application/Components/GamePlay/Character/Enemy/EnemyAIController.h"

GameObject* EnemySpawner::Spawn(const std::string& enemyId, const Math::Vector3& position)
{
	if (!catalog_.Contains(enemyId)) {
		assert(false && "unknown enemyId");
		return nullptr;
	}
	// ターゲットは敵自身がSceneContext::player経由で探すため、生成後の設定は不要
	return catalog_.Spawn(objectManager_, enemyId, position);
}

void EnemySpawner::Despawn(GameObject* enemy)
{
	if (enemy == nullptr) return;

	// 道連れオブジェクトや演出の後始末はEnemyAIControllerにまとまっているので、それを使う
	if (EnemyAIController* ai = enemy->GetComponent<EnemyAIController>()) {
		ai->RequestDespawn();
		return;
	}
	objectManager_.Destroy(enemy);
}
