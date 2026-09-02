#include "EnemyFactory.h"

#include "../../Components/Character/Enemy/EnemyDefinition.h"
#include "../../Components/Character/Enemy/EnemyAIController.h"
#include "../../Components/Character/Enemy/Brute/BruteBehavior.h"
#include "../../Components/Character/Enemy/Warrock/WarrockBehavior.h"
#include "../../Components/Character/Enemy/LockOnTargetComponent.h"
#include "../../Components/Character/Data/PostureComponent.h"
#include "../../Components/Character/Data/HealthComponent.h"
#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Transform/AttachToSocketComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/FacingDirectionComponent.h"
#include "../../Components/Collision/ColliderComponent.h"
#include "../../Components/Collision/CharacterCollisionDefaults.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Collision/WireFrameComponent.h"
#include "../../Components/Collision/AttackSourceComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Animation/BoneSocketComponent.h"
#include "../../Components/Animation/ModelAnimatorComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Sensors/GroundSensorComponent.h"
#include "../../Components/UI/Enemy/EnemyHPBarComponent.h"


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
		transform->SetScale(def.modelScale);
		enemy->AddComponent<MovementComponent>(def.moveSpeed);
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
	// コンポーネント自体はEnemyAIController1種類に統合されているため、
	// def.type(EnemyType)を見て切り替えるのは中へ渡すIEnemyBehaviorの
	// 実装だけになる(EnemyAIController.h冒頭コメント参照)。以前は
	// IEnemyAIController経由でEnemyAIController/WarrockAIControllerという
	// 別々のコンポーネント型を出し分けていたが、その必要が無くなった。
	EnemyAIController* CreateAIController(GameObject* enemy, const EnemyDefinition& def)
	{
		std::unique_ptr<IEnemyBehavior> behavior;
		switch (def.type) {
		case EnemyType::Warrock:
			behavior = std::make_unique<WarrockBehavior>();
			break;
		case EnemyType::Brute:
		default:
			behavior = std::make_unique<BruteBehavior>();
			break;
		}
		return enemy->AddComponent<EnemyAIController>(def.aiData, std::move(behavior));
	}

	// --- 当たり判定(HurtBox+Body) -------------------------------------
	void CreateColliders(GameObject* enemy, const EnemyDefinition& def)
	{
		ColliderComponent* collider = enemy->AddComponent<ColliderComponent>();
		CollisionShapeEntry& hurtBox = collider->AddCapsule("HurtBox", def.bodyRadius,
			Math::Vector3(0.0f, def.bodyRadius, 0.0f),
			Math::Vector3(0.0f, (CharacterCollisionDefaults::kFootOffset * 2) - def.bodyRadius, 0.0f),
			ColliderCategory::HurtBox, ColliderCategory::HitBox);
		hurtBox.isTrigger = true;

		using CharacterCollisionDefaults::kFootOffset;
		collider->AddCapsule("Body", def.bodyRadius,
			Math::Vector3(0.0f, kFootOffset - def.bodyRadius, 0.0f),
			Math::Vector3(0.0f, kFootOffset + def.bodyRadius, 0.0f),
			ColliderCategory::Bump);

		enemy->AddComponent<WireFrameComponent>();
	}

	// --- 物理(重力・速度・接地判定) -----------------------------------
	void CreatePhysicsComponents(GameObject* enemy)
	{
		enemy->AddComponent<GravityComponent>();
		enemy->AddComponent<VelocityComponent>();
		enemy->AddComponent<GroundSensorComponent>();
	}

	// --- 戦闘補助(向き・体幹・HP・ロックオン・アニメーター) --------------
	void CreateCombatSupportComponents(GameObject* enemy, const EnemyDefinition& def)
	{
		enemy->AddComponent<FacingDirectionComponent>();

		// 体幹(パリィ/ガードの削り合い)管理用。全敵種に一律で付けている
		// (使うかどうかはBehavior::OnHit()側の判断。BruteBehavior::
		// OnHit()参照)。
		enemy->AddComponent<PostureComponent>();

		enemy->AddComponent<HealthComponent>();
		enemy->AddComponent<LockOnTargetComponent>();

		auto* animator = enemy->AddComponent<ModelAnimatorComponent>();
		animator->SetFPS(60);
		animator->SetRootMotionBoneName("mixamorig:Hips");
		animator->SetRootMotionForwardAxis(RootMotionAxis::Y, -1.0f);
		animator->SetRootMotionScale(0.01f * def.modelScale.x);
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

		weapon->AddComponent<WireFrameComponent>();

		return weapon;
	}

	// --- 武器の生成・取り付け・道連れ登録 ------------------------------
	// 以前はIEnemyAIController経由で扱っていたが、コンポーネント自体が
	// EnemyAIController1種類に統合されたため、直接EnemyAIController*を
	// 受け取る形に戻せる(EnemyAIController.h参照)。
	void AttachWeaponAndRegister(ObjectManager& objectManager, GameObject* enemy,
		EnemyAIController* ai, Handle<SkeletonComponent>& skeletonHandle)
	{
		GameObject* weaponSocket = CreateWeaponSocket(objectManager, skeletonHandle);
		Handle<TransformComponent> weaponAttachPoint(weaponSocket->GetComponent<BoneSocketComponent>());

		GameObject* weapon = CreateWeapon(objectManager, enemy, weaponAttachPoint);
		if (weapon != nullptr) {
			ai->SetWeapon(
				Handle<ColliderComponent>(weapon->GetComponent<ColliderComponent>()),
				Handle<AttackSourceComponent>(weapon->GetComponent<AttackSourceComponent>()));
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

	// 【順序が重要】CreateCombatSupportComponents()(ModelAnimatorComponentを
	// 追加する)は、EnemyAIController(ルートモーション消費のため
	// ModelAnimatorComponent::Update()が自分より先に走っている前提。
	// EnemyAIController.h::ApplyRootMotion()コメント参照)より前に呼ぶ。
	CreateCombatSupportComponents(enemy, def);

	EnemyAIController* ai = CreateAIController(enemy, def);
	CreateColliders(enemy, def);
	CreatePhysicsComponents(enemy);

	Handle<SkeletonComponent> skeletonHandle(skeleton);
	AttachWeaponAndRegister(objectManager, enemy, ai, skeletonHandle);

	return enemy;
}