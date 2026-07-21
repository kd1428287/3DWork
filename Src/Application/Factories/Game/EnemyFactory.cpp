#include "EnemyFactory.h"
#include "EnemyDefinition.h"


#include "../../Components/Character/Enemy/EnemyStatusController.h"
#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Collision/ColliderComponent.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Sensors/GroundSensorComponent.h"


EnemyFactory::EnemyFactory(const std::unordered_map<std::string, EnemyDefinition>& database) {
	for (const auto& [id, def] : database) {
		const EnemyDefinition* defPtr = &def; 

		registry_.Register(id, [defPtr](ObjectManager& objectManager, Math::Vector3 position) {
			return BuildEnemy(objectManager, *defPtr, position);
			});
	}
}

GameObject* EnemyFactory::BuildEnemy(ObjectManager& objectManager, const EnemyDefinition& def, const Math::Vector3& position) {
	GameObject* enemy = objectManager.Instantiate(def.name);

	TransformComponent* transform = enemy->AddComponent<TransformComponent>();
	transform->SetPosition(position);

	enemy->AddComponent<MovementComponent>(def.moveSpeed);

	// EnemyStatusController::Start()内でMovementComponentへ
	// SetMovementSource(this)する。GetComponent<T>()はマップ参照なので、
	// AddComponentの順序には依存しない。
	enemy->AddComponent<EnemyStatusController>(def.patrolDistance);

	// 被弾判定。Hurtboxレイヤーで追加すると、ColliderComponentの
	// 既定ロジックによりcollideMaskが自動的にDamage/DamageLineだけを
	// 見るようになる(ColliderComponent::DefaultCollideMaskFor参照)。
	ColliderComponent* collider = enemy->AddComponent<ColliderComponent>();
	collider->AddSphere("body", def.bodyRadius, {}, ColliderLayer::HurtBox);

	enemy->AddComponent<VelocityComponent>();
	enemy->AddComponent<GravityComponent>();
	enemy->AddComponent<GroundSensorComponent>();

	enemy->AddComponent<ModelRenderComponent>();

	return enemy;
}