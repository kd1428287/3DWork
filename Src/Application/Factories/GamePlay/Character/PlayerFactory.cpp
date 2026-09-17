#include "PlayerFactory.h"
#include "Application/Definitions/Loaders/PlayerDefinitionLoader.h"
#include "CharacterFactoryUtil.h"

#include "Application/Components/Core/TransformComponent.h"
#include "Application/Components/Core/AttachToSocketComponent.h"
#include "Application/Components/Core/BoneSocketComponent.h"

#include "Application/Components/GamePlay/Character/Common/WeaponSetComponent.h"
#include "Application/Components/GamePlay/Character/Common/WeaponComponent.h"
#include "Application/Components/GamePlay/Character/Common/AttackSourceComponent.h"
#include "Application/Components/GamePlay/Character/Common/HitReactionComponent.h"
#include "Application/Components/GamePlay/Character/Common/PostureComponent.h"
#include "Application/Components/GamePlay/Character/Common/HealthComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerInputComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerStatusController.h"
#include "Application/Components/GamePlay/Character/Player/PlayerLockOnComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerMovementAnimationComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerCombatMovementComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerFacingComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerAttackSelector.h"
#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"

#include "Application/Components/Physics/Movement/MovementResolverComponent.h"
#include "Application/Components/Physics/Movement/MovementComponent.h"
#include "Application/Components/Physics/Movement/VelocityComponent.h"
#include "Application/Components/Physics/Movement/GravityComponent.h"
#include "Application/Components/Physics/Sensors/GroundSensorComponent.h"

#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/PolygonRenderComponent.h" 
#include "Application/Components/Graphics/Effect/SlashTrailComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/TwoBoneIKComponent.h"
#include "Application/Components/Graphics/Animation/RootMotionApplierComponent.h"
#include "Application/Components/Graphics/Animation/FacingDirectionComponent.h"

#include "Application/Components/Graphics/Render/WireFrameComponent.h"

#include "Application/Definitions/Character/Common/CharacterCollisionDefaults.h"

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
		animator->SetSpeedScale(visuals.animatorScale);
		animator->SetRootMotionBoneName(visuals.rootMotionBoneName);
		animator->SetRootMotionForwardAxis(visuals.rootMotionAxis, visuals.rootMotionAxisSign);
		animator->SetRootMotionScale(visuals.rootMotionScale);

		player->AddComponent<RootMotionApplierComponent>();

		return skeleton;
	}

	// 物理・当たり判定関連のセットアップ
	// 注: コライダー座標はJSONのstart/endをそのまま使う(モデル原点は足元中心のため、
	// 過去にあった+Vector3(0,1,0)のオフセットやHurtBox側だけstartを無視する分岐は
	// モデル原点をモデル中心と誤認していたデバッグ用の暫定対応だったため撤去済み)。
	ColliderComponent* AttachPhysics(GameObject* player, const PlayerCombatStatsDefinition& combatStats,
		const std::vector<CapsuleColliderDefinition>& colliderDefs, const IKChainDefinition& rightArmIK,
		const PlayerMovementAnimationDefinition& movementAnimations)
	{
		player->AddComponent<GravityComponent>();
		player->AddComponent<VelocityComponent>(0.01f);
		auto* collider = player->AddComponent<ColliderComponent>();
		player->AddComponent<GroundSensorComponent>();
		player->AddComponent<PlayerMovementAnimationComponent>()->SetMovementAnimations(movementAnimations);
		player->AddComponent<FacingDirectionComponent>();

		// 体幹管理用
		auto* posture = player->AddComponent<PostureComponent>();

		// HP管理用
		auto* health = player->AddComponent<HealthComponent>();
		health->SetMax(combatStats.maxHealth, true);

		player->AddComponent<PlayerLockOnComponent>();
		player->AddComponent<HitReactionComponent>();
		player->AddComponent<WeaponSetComponent>();

		for (const auto& def : colliderDefs) {
			if (def.interactsWith != ColliderCategory::None) {
				collider->AddCapsule(def.name, def.radius, def.start, def.end, def.category, def.interactsWith);
			}
			else {
				collider->AddCapsule(def.name, def.radius, def.start, def.end, def.category);
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
		player->AddComponent<MovementResolverComponent>();
	
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
	AttachPhysics(player, definition.combatStats, definition.colliders, definition.rightArmIK,
		definition.movementAnimations);
	AttachMovement(player, definition.walkSpeed);

	player->AddComponent<PlayerStatusController>()->SetEvadeAndGuardData(
		definition.combatBehavior.evade, definition.combatBehavior.guard);
	player->AddComponent<CameraTargetComponent>()->SetOffset({ 0.f, CharacterCollisionDefaults::kEyeHeight, 0.f });
	player->AddComponent<WireFrameComponent>();
	player->AddComponent<PlayerCombatMovementComponent>()->SetMovementSpeeds(definition.walkSpeed, definition.runSpeed);
	player->AddComponent<PlayerAttackSelector>()->SetAttackTable(definition.combatBehavior.attackTable);
	player->AddComponent<PlayerFacingComponent>();

	// --- 腕・武器のソケット生成 -----------------------------------------
	Handle<SkeletonComponent> skeletonHandle(skeleton);

	for (const std::string& boneName : definition.auxiliarySocketBones) {
		CharacterFactoryUtil::CreateSocket(objectManager, boneName, skeletonHandle);
	}

	// 武器生成用に武器ソケットのみ参照を受け取る
	GameObject* weaponSocket = CharacterFactoryUtil::CreateSocket(objectManager, definition.weaponSocketBone, skeletonHandle);
	if (weaponSocket == nullptr) return player;
	Handle<TransformComponent> weaponAttachPoint(weaponSocket->GetComponent<BoneSocketComponent>());

	// --- 武器の生成とStatusControllerへの登録 ----------------------------
	GameObject* weapon = CreateWeapon(objectManager, player, weaponAttachPoint, definition.weapon, definition.rightArmIK);

	if (weapon != nullptr) {
		player->GetComponent<PlayerStatusController>()->SetWeapon(
			Handle<WeaponComponent>(weapon->GetComponent<WeaponComponent>()));
	}

	return player;
}

GameObject* PlayerFactory::CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle,
	const WeaponDefinition& weaponDefinition, const IKChainDefinition& ikChain)
{
	GameObject* weapon = objectManager.Instantiate("weapon");
	if (!weapon) return nullptr;

	auto* transform = weapon->AddComponent<TransformComponent>();

	auto* socket = weapon->AddComponent<AttachToSocketComponent>(handle);
	socket->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
		DirectX::XMConvertToRadians(weaponDefinition.socketLocalEulerRotationDeg.x),
		DirectX::XMConvertToRadians(weaponDefinition.socketLocalEulerRotationDeg.y),
		DirectX::XMConvertToRadians(weaponDefinition.socketLocalEulerRotationDeg.z)));

	socket->SetLocalPositon(weaponDefinition.socketLocalPosition);

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

	auto* trail = weapon->AddComponent<SlashTrailComponent>("Sword");
	trail->SetBaseTip(Math::Vector3{ 0,0,-0.75 }, Math::Vector3{ 0,0,-2.25 });
	trail->SetBaseTip(Math::Vector3{ 0,0,-0.5 }, Math::Vector3{ 0,0,-1.5f });
	trail->SetKey("Sword_Player");
	trail->StartEmit();

	weapon->AddComponent<WeaponComponent>();

	return weapon;
}
