#include "PlayerFactory.h"
#include "PlayerDefinitionLoader.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Transform/SocketComponent.h"
#include "../../Components/Transform/AttachToSocketComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/VelocityComponent.h"
#include "../../Components/Movement/FacingDirectionComponent.h"
#include "../../Components/Character/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Render/PolygonRenderComponent.h" 
#include "../../Components/Effect/TrailPolygonComponent.h"
#include "../../Components/Animation/ModelAnimatorComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Animation/BoneSocketComponent.h"
#include "../../Components/Animation/TwoBoneIKComponent.h"
#include "../../Components/Character/Player/PlayerStatusController.h"
#include "../../Components/Character/Player/PlayerLockOnComponent.h"
#include "../../Components/Character/Data/PostureComponent.h"
#include "../../Components/Character/Data/HealthComponent.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Collision/CharacterCollisionDefaults.h"
#include "../../Components/Collision/AttackSourceComponent.h"
#include "../../Components/Collision/WireFrameComponent.h"
#include "../../Components/Sensors/GroundSensorComponent.h"
#include "../../Components/UI/Player/PlayerStatusUIComponent.h"
#include "../../Components/Effect/TrailPolygonComponent.h"

namespace
{
	// 見た目・描画・アニメーション関連のセットアップ
	SkeletonComponent* AttachVisuals(GameObject* player, const PlayerVisualDefinition& visuals)
	{
		auto* transform = player->AddComponent<TransformComponent>();
		transform->SetPosition({ 0.f, 0.f, 0.f });
		player->AddComponent<ModelRenderComponent>();

		auto* skeleton = player->AddComponent<SkeletonComponent>();
		skeleton->SetModelData(visuals.modelPath);

		auto* animator = player->AddComponent<ModelAnimatorComponent>();
		animator->SetFPS(visuals.animatorFPS);
		animator->SetRootMotionBoneName(visuals.rootMotionBoneName);
		animator->SetRootMotionForwardAxis(visuals.rootMotionAxis, visuals.rootMotionAxisSign);
		animator->SetRootMotionScale(visuals.rootMotionScale);

		return skeleton;
	}

	// 物理・当たり判定関連のセットアップ
	ColliderComponent* AttachPhysics(GameObject* player, const PlayerCombatStatsDefinition& combatStats,
		const std::vector<CapsuleColliderDefinition>& colliderDefs, const IKChainDefinition& rightArmIK)
	{
		player->AddComponent<GravityComponent>();
		player->AddComponent<VelocityComponent>();
		auto* collider = player->AddComponent<ColliderComponent>();
		player->AddComponent<GroundSensorComponent>();
		player->AddComponent<FacingDirectionComponent>();

		// 体幹管理用
		auto* posture = player->AddComponent<PostureComponent>();

		// HP管理用
		auto* health = player->AddComponent<HealthComponent>();
		health->SetMax(combatStats.maxHealth, true);

		//player->AddComponent<PlayerStatusUIComponent>();

		player->AddComponent<PlayerLockOnComponent>();

		//player->AddComponent<TwoBoneIKComponent>(
			//rightArmIK.rootBone, rightArmIK.midBone, rightArmIK.tipParentBone, rightArmIK.tipBone);

		for (const auto& def : colliderDefs) {
			CollisionShapeEntry* shape = nullptr;
			if (def.interactsWith != ColliderCategory::None) {
				shape = &collider->AddCapsule(def.name, def.radius, def.start, def.end, def.category, def.interactsWith);
			}
			else {
				shape = &collider->AddCapsule(def.name, def.radius, def.start, def.end, def.category);
			}

			if (def.isTrigger) {
				collider->SetShapeIsTrigger(def.name, true);
			}
		}

		return collider;
	}

	// 入力・移動関連のセットアップ
	void AttachMovement(GameObject* player, float walkSpeed)
	{
		auto* input = player->AddComponent<PlayerInputComponent>();
		auto* move = player->AddComponent<MovementComponent>(walkSpeed);
		move->SetMovementSource(input);
	}
}

GameObject* PlayerFactory::CreatePlayer(ObjectManager& objectManager, const std::string& definitionPath)
{
	PlayerDefinition definition;
	if (!PlayerDefinitionLoader::LoadFromFile(definitionPath, definition)) {
		return nullptr;
	}
	return CreatePlayer(objectManager, definition);
}

GameObject* PlayerFactory::CreatePlayer(ObjectManager& objectManager, const PlayerDefinition& definition)
{
	GameObject* player = objectManager.Instantiate("player");
	if (!player) return nullptr;

	SkeletonComponent* skeleton = AttachVisuals(player, definition.visuals);
	AttachPhysics(player, definition.combatStats, definition.colliders, definition.rightArmIK);
	AttachMovement(player, definition.walkSpeed);

	player->AddComponent<PlayerStatusController>();
	player->AddComponent<CameraTargetComponent>()->SetLocalPosition({ 0.f,0.5f,0.f });

	// --- 腕・武器のソケット生成 -----------------------------------------
	Handle<SkeletonComponent> skeletonHandle(skeleton);

	for (const std::string& boneName : definition.auxiliarySocketBones) {
		CreateSocket(objectManager, boneName, skeletonHandle);
	}

	// 武器生成用に武器ソケットのみ参照を受け取る
	GameObject* weaponSocket = CreateSocket(objectManager, definition.weaponSocketBone, skeletonHandle);
	if (weaponSocket == nullptr) return player;
	Handle<TransformComponent> weaponAttachPoint(weaponSocket->GetComponent<BoneSocketComponent>());

	// --- 武器の生成とStatusControllerへの登録 ----------------------------
	GameObject* weapon = CreateWeapon(objectManager, player, weaponAttachPoint, definition.weapon, definition.rightArmIK);

	// 武器のColliderComponent/AttackSourceComponentへの参照をStatusControllerに登録する。
	if (weapon != nullptr) {
		player->GetComponent<PlayerStatusController>()->SetWeapon(
			Handle<ColliderComponent>(weapon->GetComponent<ColliderComponent>()),
			Handle<AttackSourceComponent>(weapon->GetComponent<AttackSourceComponent>()),
			Handle<TrailPolygonComponent>(weapon->GetComponent<TrailPolygonComponent>()));
	}

	return player;
}

GameObject* PlayerFactory::CreateSocket(ObjectManager& objectManager, std::string objID, Handle<SkeletonComponent>& handle)
{
	auto* obj = objectManager.Instantiate(objID);
	if (!obj) return nullptr;
	obj->AddComponent<BoneSocketComponent>(handle, objID);
	return obj;
}

GameObject* PlayerFactory::CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle,
	const WeaponDefinition& weaponDefinition, const IKChainDefinition& ikChain)
{
	GameObject* weapon = objectManager.Instantiate("weapon");
	if (!weapon) return nullptr;

	auto* transform = weapon->AddComponent<TransformComponent>();

	auto* socket = weapon->AddComponent<AttachToSocketComponent>(handle);
	/*socket->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
		weaponDefinition.socketLocalEulerRotationDeg.x,
		DirectX::XMConvertToRadians(weaponDefinition.socketLocalEulerRotationDeg.y),
		weaponDefinition.socketLocalEulerRotationDeg.z));

	socket->SetLocalPositon(weaponDefinition.socketLocalPosition);*/

	auto* skeleton = weapon->AddComponent<SkeletonComponent>();
	skeleton->SetModelData(weaponDefinition.modelPath);
	weapon->AddComponent<ModelRenderComponent>();

	auto* collision = weapon->AddComponent<ColliderComponent>();
	CollisionShapeEntry& hitBox = collision->AddBox(
		"HitBox", weaponDefinition.hitBox.halfExtents, weaponDefinition.hitBox.offset, ColliderCategory::HitBox);
	hitBox.enabled = false;
	hitBox.isTrigger = true;
	collision->IgnoreCollisionWith(player);

	auto* attackSource = weapon->AddComponent<AttackSourceComponent>();
	attackSource->ownerCharacter = Handle<GameObject>(player);

	weapon->AddComponent<WireFrameComponent>();

	weapon->AddComponent<TwoBoneIKComponent>(
		ikChain.rootBone, ikChain.midBone, ikChain.tipParentBone, ikChain.tipBone);

	// 攻撃の軌跡エフェクト。TrailPolygonComponentはデータ管理のみを担当し、
	// 実際の描画はPolygonRenderComponent(IPolygonRenderSourceを同じ
	// GameObject上から自動的に見つけて描画する側)が行うため、両方をセットで
	// 付ける必要がある(TrailPolygonEffectComponent.h冒頭コメント参照)。
	// 発生/停止のタイミングはPlayerStatusController::SetWeaponTrailEmitting()
	// 経由でStateAttack側から制御する(SetWeaponHitBoxEnabledと同じ窓)。
	weapon->AddComponent<PolygonRenderComponent>();
	auto* trail = weapon->AddComponent<TrailPolygonComponent>("Asset/Textures/Game/Effect/Trail.png");
	trail->StartEmit();
	trail->SetPattern(KdTrailPolygon::Trail_Pattern::eBillboard);
	//trail->SetPattern(KdTrailPolygon::Trail_Pattern::eVertices);
	trail->SetBaseTip(weaponDefinition.hitBox.offset, Math::Vector3(0,0,2.f));

	return weapon;
}
