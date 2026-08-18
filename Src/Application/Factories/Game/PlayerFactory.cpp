#include "PlayerFactory.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Transform/SocketComponent.h"
#include "../../Components/Transform/AttachToSocketComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/VelocityComponent.h"
#include "../../Components/Movement/FacingDirectionComponent.h"
#include "../../Components/Character/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Animation/ModelAnimatorComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Animation/BoneSocketComponent.h"
#include "../../Components/Character/Player/PlayerStatusController.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Collision/CharacterCollisionDefaults.h"
#include "../../Components/Collision/AttackSourceComponent.h"
#include "../../Components/Collision/WireFrameComponent.h"
#include "../../Components/Sensors/GroundSensorComponent.h"

namespace
{
	// 歩行速度。今のところ技データテーブル側にも移動速度の概念が無いため、
	// ここに定数として置いている(PlayerCombatTypes.h参照)。
	constexpr float kWalkSpeed = 2.0f;

	// 見た目・描画・アニメーション関連のセットアップ。
	// SkeletonComponentはソケット生成側でHandleとして必要になるため返す。
	SkeletonComponent* AttachVisuals(GameObject* player)
	{
		player->AddComponent<TransformComponent>()->SetPosition({ 0.f, 0.f, 0.f });
		player->AddComponent<ModelRenderComponent>();

		auto* skeleton = player->AddComponent<SkeletonComponent>();
		skeleton->SetModelData("Asset/Models/Character/Player/Player.gltf");

		auto* animator = player->AddComponent<ModelAnimatorComponent>();
		animator->SetFPS(60);
		animator->Play("SlashCombo1");

		return skeleton;
	}

	// 物理・当たり判定関連のセットアップ(重力・速度・接地判定・押し返し用Body)。
	// 呼び出し側がBodyの形状を直接いじることは今のところ無いが、将来の拡張
	// (装備品でのBody差し替え等)を見据えて一応返しておく。
	ColliderComponent* AttachPhysics(GameObject* player)
	{
		// GravityComponentをVelocityComponentより先にAddComponentしている。
		// 理由はEnemyFactory::BuildEnemy側の同種のコメント参照
		// (着地時の速度クリアが、同じフレームのVelocityComponent::Updateに
		//  よる位置反映より先に確定するようにするため。順序が逆だと着地の
		//  瞬間に地面へめり込む→押し戻される、を繰り返す不安定な挙動になる)。
		player->AddComponent<GravityComponent>();
		player->AddComponent<VelocityComponent>();
		auto* collider = player->AddComponent<ColliderComponent>();
		player->AddComponent<GroundSensorComponent>();
		player->AddComponent<FacingDirectionComponent>();
		player->AddComponent<WireFrameComponent>();

		// Y方向の半径をCharacterCollisionDefaults::kFootOffsetと一致させて
		// いる点が重要。Bodyの下端がfootOffsetまで届いていないと、Bodyが
		// 地面に乗って静止した高さとGroundSensorComponentの接地レイの
		// 基準高さがズレる(詳細はCharacterCollisionDefaults.h、および
		// EnemyFactory::BuildEnemy側の同種の修正コメント参照)。
		collider->AddBox("Body",
			Math::Vector3(0.3f, CharacterCollisionDefaults::kFootOffset, 0.25f), Math::Vector3(0.f, CharacterCollisionDefaults::kFootOffset, 0.f),
			ColliderCategory::Bump);

		return collider;
	}

	// 入力・移動関連のセットアップ。
	void AttachMovement(GameObject* player)
	{
		auto* input = player->AddComponent<PlayerInputComponent>();
		auto* move = player->AddComponent<MovementComponent>(kWalkSpeed);
		move->SetMovementSource(input);
	}
}

GameObject* PlayerFactory::CreatePlayer(ObjectManager& objectManager)
{
	// ObjectManagerがInstantiate時に内部の所有権コンテナ(std::vector<
	// std::unique_ptr<GameObject>>等)へ格納し、その生ポインタを返して
	// くれている想定。
	GameObject* player = objectManager.Instantiate("player");
	if (!player) return nullptr;

	SkeletonComponent* skeleton = AttachVisuals(player);
	AttachPhysics(player);
	AttachMovement(player);

	player->AddComponent<PlayerStatusController>();
	player->AddComponent<CameraTargetComponent>();

	// --- 腕・武器のソケット生成 -----------------------------------------
	// 武器の取り付け先(右手)以外のソケットは、現時点では取り付け先として
	// 使っていないが、将来的な装備拡張(盾・エフェクト等)や、SkeletonComponent
	// のボーン構成確認用として生成だけ済ませておく。
	Handle<SkeletonComponent> skeletonHandle(skeleton);

	static const char* const kAuxiliarySocketBones[] = {
		"mixamorig:LeftShoulder", "mixamorig:LeftArm", "mixamorig:LeftHand",
		"mixamorig:RightShoulder", "mixamorig:RightArm",
	};
	for (const char* boneName : kAuxiliarySocketBones) {
		CreateSocket(objectManager, boneName, skeletonHandle);
	}

	GameObject* rightHandSocket = CreateSocket(objectManager, "mixamorig:RightHand", skeletonHandle);
	Handle<TransformComponent> weaponAttachPoint(rightHandSocket->GetComponent<BoneSocketComponent>());

	// --- 武器の生成とStatusControllerへの登録 ----------------------------
	GameObject* weapon = CreateWeapon(objectManager, player, weaponAttachPoint);

	// 武器のColliderComponent/AttackSourceComponentへの参照をStatusController
	// に登録する。武器は別GameObject(ソケット経由でアタッチ)のため、
	// PlayerStatusController::SetWeapon()を通じて弱参照(Handle)で渡す
	// (StateAttackがAttackActiveフェーズの開始/終了に合わせてHitBoxの
	//  enabled切り替えと多段ヒット記録のクリアを行うために使う)。
	if (weapon != nullptr) {
		player->GetComponent<PlayerStatusController>()->SetWeapon(
			Handle<ColliderComponent>(weapon->GetComponent<ColliderComponent>()),
			Handle<AttackSourceComponent>(weapon->GetComponent<AttackSourceComponent>()));
	}

	// 所有権を持たない「利用権(参照用)」としての生ポインタを返す。
	return player;
}

GameObject* PlayerFactory::CreateSocket(ObjectManager& objectManager, std::string objID, Handle<SkeletonComponent>& handle)
{
	auto* obj = objectManager.Instantiate(objID);
	if (!obj) return nullptr;
	obj->AddComponent<BoneSocketComponent>(handle, objID);
	return obj;
}

GameObject* PlayerFactory::CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle)
{
	GameObject* weapon = objectManager.Instantiate("weapon");
	if (!weapon) return nullptr;

	auto* transform = weapon->AddComponent<TransformComponent>();
	transform->SetScale({ 0.5f, 0.5f, 0.5f });

	weapon->AddComponent<AttachToSocketComponent>(handle);

	auto* skeleton = weapon->AddComponent<SkeletonComponent>();
	//skeleton->SetModelData("Asset/Models/Character/Player/box.gltf");
	weapon->AddComponent<ModelRenderComponent>();

	auto* collision = weapon->AddComponent<ColliderComponent>();
	// 常時enabled=trueのままだと、HurtBoxと重なっている間ずっと
	// CollisionEnterEventが発火し続け、ノックバックが際限なく積み増される
	// (実際に発生していた不具合)。攻撃発生フレーム(AttackActive)の間だけ
	// PlayerStatusController::SetWeaponHitBoxEnabled()経由で有効化する
	// 前提のため、生成直後はfalseにしておく。
	CollisionShapeEntry& hitBox = collision->AddBox(
		"HitBox", Math::Vector3(1.f, 1.f, 1.f), Math::Vector3(0.f, 1.f, 0.f), ColliderCategory::HitBox);
	hitBox.enabled = false;
	hitBox.isTrigger = true;
	collision->IgnoreCollisionWith(player);

	weapon->AddComponent<AttackSourceComponent>();
	weapon->AddComponent<WireFrameComponent>();

	return weapon;
}