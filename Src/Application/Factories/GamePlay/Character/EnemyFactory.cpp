#include "EnemyFactory.h"

#include "Application/Definitions/Character/Enemy/EnemyDefinition.h"
#include "Application/Definitions/Character/Common/CharacterCollisionDefaults.h"

#include "Application/Components/GamePlay/Character/Enemy/EnemyAIController.h"
#include "Application/Components/GamePlay/Character/Enemy/Brute/BruteBehavior.h"
#include "Application/Components/GamePlay/Character/Enemy/Warrock/WarrockBehavior.h"
#include "Application/Components/GamePlay/Character/Enemy/LockOnTargetComponent.h"
#include "Application/Components/GamePlay/Character/Common/PostureComponent.h"
#include "Application/Components/GamePlay/Character/Common/HealthComponent.h"
#include "Application/Components/GamePlay/Character/Common/AttackSourceComponent.h"
#include "Application/Components/GamePlay/Character/Common/WeaponSetComponent.h"
#include "Application/Components/GamePlay/Character/Common/WeaponComponent.h"
#include "Application/Components/GamePlay/Character/Common/HitReactionComponent.h"

#include "Application/Components/Core/AttachToSocketComponent.h"
#include "Application/Components/Core/BoneSocketComponent.h"

#include "Application/Components/Physics/Movement/MovementComponent.h"
#include "Application/Components/Physics/Movement/MovementResolverComponent.h"
#include "Application/Components/Physics/Movement/GravityComponent.h"
#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Sensors/GroundSensorComponent.h"

#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/FacingDirectionComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/WireFrameComponent.h"
#include "Application/Components/Graphics/UI/GamePlay/EnemyWorldGaugeComponent.h"

namespace
{
	// ============================================================
	// BuildEnemy()を構成する各ステップ。責務ごとに分けることで、
	// BuildEnemy()自体は「どの順で何を組み立てるか」の見取り図として
	// 読めるようにする(既存のCreateWeaponSocket/CreateWeaponと同じ
	// 匿名名前空間ヘルパーの流儀に揃えた)。
	// ============================================================

	// --- Transform / Movement ---------------------------------------
	void CreateTransformAndMovement(GameObject* enemy, const EnemyDefinition& def, const Math::Vector3& position)
	{
		TransformComponent* transform = enemy->AddComponent<TransformComponent>();
		transform->SetPosition(position);
		transform->SetScale(def.GetModelScale());

		enemy->AddComponent<MovementComponent>();
	}

	// --- 見た目(スケルトン+モデル描画) --------------------------------
	// 武器をボーンソケット経由で手に追従させるため、SkeletonComponentの
	// ポインタを呼び出し側(BuildEnemy)へ返す(武器ソケット生成時に使う)。
	SkeletonComponent* AttachVisuals(GameObject* enemy, const EnemyDefinition& def)
	{
		SkeletonComponent* skeleton = enemy->AddComponent<SkeletonComponent>();
		skeleton->SetModelData(def.modelPath);
		enemy->AddComponent<ModelRenderComponent>();
		return skeleton;
	}

	// --- 意思決定・実行(EnemyAIController + IEnemyBehavior) --------------
	EnemyAIController* CreateAIController(GameObject* enemy, const EnemyDefinition& def)
	{
		std::unique_ptr<IEnemyBehavior> behavior;
		switch (def.type) {
		case EnemyType::Warrock:
			behavior = std::make_unique<WarrockBehavior>();
			break;
		case EnemyType::Brute:
		default:
			//behavior = std::make_unique<BruteBehavior>();
			break;
		}
		return enemy->AddComponent<EnemyAIController>(def.aiData, std::move(behavior));
	}

	// --- 当たり判定(HurtBox+Body) -------------------------------------
	void CreateColliders(GameObject* enemy, const EnemyDefinition& def)
	{
		const float colliderRadius = def.GetColliderRadius();

		ColliderComponent* collider = enemy->AddComponent<ColliderComponent>();
		CollisionShapeEntry& hurtBox = collider->AddCapsule("HurtBox", colliderRadius,
			Math::Vector3(0.0f, colliderRadius, 0.0f),
			Math::Vector3(0.0f, (CharacterCollisionDefaults::kFootOffset * 2) - colliderRadius, 0.0f),
			ColliderCategory::HurtBox, ColliderCategory::HitBox);
		hurtBox.isTrigger = true;

		using CharacterCollisionDefaults::kFootOffset;
		collider->AddCapsule("Body", colliderRadius,
			Math::Vector3(0.0f, kFootOffset - colliderRadius, 0.0f),
			Math::Vector3(0.0f, kFootOffset + colliderRadius, 0.0f),
			ColliderCategory::Bump);
	}

	// --- 物理(重力・速度・接地判定) -----------------------------------
	void CreatePhysicsComponents(GameObject* enemy)
	{
		enemy->AddComponent<GravityComponent>();
		enemy->AddComponent<VelocityComponent>();
		enemy->AddComponent<TweenMoveComponent>();
		enemy->AddComponent<MovementResolverComponent>();
		enemy->AddComponent<GroundSensorComponent>();
	}

	// --- 戦闘補助(向き・体幹・HP・ロックオン・アニメーター) --------------
	void CreateCombatSupportComponents(GameObject* enemy, const EnemyDefinition& def)
	{
		auto* facing = enemy->AddComponent<FacingDirectionComponent>();
		facing->SetRotationSpeed(3.f);

		auto* posture = enemy->AddComponent<PostureComponent>(100);
	/*	posture->SetLowerLimit();
		posture->SetRegenPerSecond();
		posture->SetRegenDelaySeconds();*/

		auto* health = enemy->AddComponent<HealthComponent>();
		health->SetMax(1000.f, true);
		enemy->AddComponent<LockOnTargetComponent>();

		
		enemy->AddComponent<WeaponSetComponent>();
		auto* hitReaction = enemy->AddComponent<HitReactionComponent>();
		hitReaction->SetConfig(def.hitReactionConfig);

		auto* animator = enemy->AddComponent<ModelAnimatorComponent>();
		animator->SetFPS(60);
		animator->SetRootMotionBoneName("mixamorig:Hips");
		animator->SetRootMotionForwardAxis(RootMotionAxis::Y, -1.0f);
		animator->SetRootMotionScale(0.01f * def.GetModelScale().x);
	}

	// --- 頭上ゲージUIの生成 --------------------------------------------
	// Enemyとは別のGameObjectとして生成し、Handle<GameObject>経由で
	// イベント購読する(GaugeWatcher/EnemyWorldGaugeComponent参照)。
	void CreateWorldGauge(ObjectManager& objectManager, GameObject* enemy)
	{
		GameObject* gauge = objectManager.Instantiate("enemy_gauge");
		if (!gauge) return;

		gauge->AddFlag(ObjectFlags::UI);
		//gauge->AddComponent<EnemyWorldGaugeComponent>()->SetTarget(Handle<GameObject>(enemy));
	}

	// --- 武器のソケット生成 --------------------------------------------
	GameObject* CreateWeaponSocket(ObjectManager& objectManager, Handle<SkeletonComponent>& skeletonHandle) {
		GameObject* socket = objectManager.Instantiate("enemy_weapon_socket");
		if (!socket) return nullptr;
		socket->AddComponent<BoneSocketComponent>(skeletonHandle, "mixamorig:RightHand");
		return socket;
	}

	// --- 武器本体の生成 --------------------------------------------------
	GameObject* CreateWeapon(ObjectManager& objectManager, GameObject* enemy, Handle<TransformComponent>& attachPoint) {
		GameObject* weapon = objectManager.Instantiate("enemy_weapon");
		if (!weapon) return nullptr;

		auto* transform = weapon->AddComponent<TransformComponent>();
		transform->SetScale({ 0.5f, 0.5f, 0.5f });

		weapon->AddComponent<AttachToSocketComponent>(attachPoint);

		auto* collision = weapon->AddComponent<ColliderComponent>();
		CollisionShapeEntry& hitBox = collision->AddBox(
			"HitBox", Math::Vector3(5.f, 5.f, 5.1f), Math::Vector3(1.f, 1.f, 0.f), ColliderCategory::HitBox);
		hitBox.enabled = false;
		hitBox.isTrigger = true;
		collision->IgnoreCollisionWith(enemy);

		auto* attackSource = weapon->AddComponent<AttackSourceComponent>();
		attackSource->ownerCharacter = Handle<GameObject>(enemy);

	
		weapon->AddComponent<WeaponComponent>();

		// デバッグ用判定可視化コンポーネント
		weapon->AddComponent<WireFrameComponent>();

		return weapon;
	}

	// --- 武器の生成・取り付け・道連れ登録 ------------------------------
	void AttachWeaponAndRegister(ObjectManager& objectManager, GameObject* enemy,
		EnemyAIController* ai, Handle<SkeletonComponent>& skeletonHandle)
	{
		GameObject* weaponSocket = CreateWeaponSocket(objectManager, skeletonHandle);
		Handle<TransformComponent> weaponAttachPoint(weaponSocket->GetComponent<BoneSocketComponent>());

		GameObject* weapon = CreateWeapon(objectManager, enemy, weaponAttachPoint);
		if (weapon != nullptr) {
			ai->SetWeapon(Handle<WeaponComponent>(weapon->GetComponent<WeaponComponent>()));
		}

		ai->RegisterOwnedObject(Handle<GameObject>(weaponSocket));
		if (weapon != nullptr) {
			ai->RegisterOwnedObject(Handle<GameObject>(weapon));
		}
	}
}

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

	CreateTransformAndMovement(enemy, def, position);
	SkeletonComponent* skeleton = AttachVisuals(enemy, def);

	CreateCombatSupportComponents(enemy, def);

	EnemyAIController* ai = CreateAIController(enemy, def);
	CreateColliders(enemy, def);
	CreatePhysicsComponents(enemy);
	CreateWorldGauge(objectManager, enemy);

	Handle<SkeletonComponent> skeletonHandle(skeleton);
	AttachWeaponAndRegister(objectManager, enemy, ai, skeletonHandle);

	return enemy;
}