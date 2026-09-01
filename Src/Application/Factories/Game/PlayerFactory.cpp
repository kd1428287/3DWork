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
#include "../../Components/Render/PolygonRenderComponent.h" 
#include "../../Components/Effect/TrailPolygonEffectComponent.h"
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
		animator->SetRootMotionBoneName("mixamorig:Hips");
		animator->SetRootMotionForwardAxis(RootMotionAxis::Y, -1.0f);
		animator->SetRootMotionScale(0.01f);

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

		// 体幹(パリィ/ガードの削り合い)管理用。数値の詳細(最大値・
		// 回復速度等)はPostureComponentのデフォルト値のまま、別途調整する。
		auto* posture = player->AddComponent<PostureComponent>();

		// HP管理用。数値の詳細(最大値等)はHealthComponentのデフォルト値
		// のまま、別途調整する。
		auto* health = player->AddComponent<HealthComponent>();
		health->SetMax(100.f, true);

		//player->AddComponent<PlayerStatusUIComponent>();

		player->AddComponent<PlayerLockOnComponent>();

		player->AddComponent<TwoBoneIKComponent>("mixamorig:RightShoulder", "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand");

		// Y方向の半径をCharacterCollisionDefaults::kFootOffsetと一致させて
		// いる点が重要。Bodyの下端がfootOffsetまで届いていないと、Bodyが
		// 地面に乗って静止した高さとGroundSensorComponentの接地レイの
		// 基準高さがズレる(詳細はCharacterCollisionDefaults.h、および
		// EnemyFactory::BuildEnemy側の同種の修正コメント参照)。
		collider->AddCapsule("Body", 0.4,
			Math::Vector3(0.0f, 0.4, 0.0f),
			Math::Vector3(0.0f, (CharacterCollisionDefaults::kFootOffset * 2) - 0.4, 0.0f),
			ColliderCategory::Bump);

		collider->AddCapsule("HurtBox", 0.3,
			Math::Vector3(0.0f, 0.3, 0.0f),
			Math::Vector3(0.0f, (CharacterCollisionDefaults::kFootOffset * 2) - 0.3, 0.0f),
			ColliderCategory::HurtBox, ColliderCategory::HitBox);

		collider->SetShapeIsTrigger("HurtBox", true);

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
	player->AddComponent<CameraTargetComponent>()->SetLocalPosition({ 0.f,0.5f,0.f });
	// --- 腕・武器のソケット生成 -----------------------------------------
		// 武器の取り付け先(右手)以外のソケットは、現時点では取り付け先として
		// 使っていないが、将来的な装備拡張(盾・エフェクト等)や、SkeletonComponent
		// のボーン構成確認用として生成だけ済ませておく。
	Handle<SkeletonComponent> skeletonHandle(skeleton);

	static const char* const kAuxiliarySocketBones[] = {
		//"mixamorig:HeadTop_End", "mixamorig:Head", "mixamorig:Neck",
		//"mixamorig:LeftHandThumb4", "mixamorig:LeftHandThumb3", "mixamorig:LeftHandThumb2", "mixamorig:LeftHandThumb1",
		//"mixamorig:LeftHandIndex4", "mixamorig:LeftHandIndex3", "mixamorig:LeftHandIndex2", "mixamorig:LeftHandIndex1",
		//"mixamorig:LeftHandMiddle4", "mixamorig:LeftHandMiddle3", "mixamorig:LeftHandMiddle2", "mixamorig:LeftHandMiddle1",
		//"mixamorig:LeftHandRing4", "mixamorig:LeftHandRing3", "mixamorig:LeftHandRing2", "mixamorig:LeftHandRing1",
		//"mixamorig:LeftHandPinky4", "mixamorig:LeftHandPinky3", "mixamorig:LeftHandPinky2", "mixamorig:LeftHandPinky1",
		"mixamorig:LeftHand", "mixamorig:LeftForeArm", "mixamorig:LeftArm", "mixamorig:LeftShoulder",
		//"mixamorig:RightHandThumb4", "mixamorig:RightHandThumb3", "mixamorig:RightHandThumb2", "mixamorig:RightHandThumb1",
		//"mixamorig:RightHandIndex4", "mixamorig:RightHandIndex3", "mixamorig:RightHandIndex2", "mixamorig:RightHandIndex1",
		//"mixamorig:RightHandMiddle4", "mixamorig:RightHandMiddle3", "mixamorig:RightHandMiddle2", "mixamorig:RightHandMiddle1",
		//"mixamorig:RightHandRing4", "mixamorig:RightHandRing3", "mixamorig:RightHandRing2", "mixamorig:RightHandRing1",
		//"mixamorig:RightHandPinky4", "mixamorig:RightHandPinky3", "mixamorig:RightHandPinky2", "mixamorig:RightHandPinky1",
		// //mixamorig:RightHand は直後で個別に生成・取得するため除外
		//"mixamorig:RightForeArm", "mixamorig:RightArm", "mixamorig:RightShoulder",
		//"mixamorig:Spine2", "mixamorig:Spine1", "mixamorig:Spine",
		//"mixamorig:LeftToe_End", "mixamorig:LeftToeBase", "mixamorig:LeftFoot", "mixamorig:LeftLeg", "mixamorig:LeftUpLeg",
		//"mixamorig:RightToe_End", "mixamorig:RightToeBase", "mixamorig:RightFoot", "mixamorig:RightLeg", "mixamorig:RightUpLeg",
		//"mixamorig:Hips"
	};
	for (const char* boneName : kAuxiliarySocketBones) {
		CreateSocket(objectManager, boneName, skeletonHandle);
	}

	// 武器生成用に RightHand ソケットのみ参照を受け取る
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
			Handle<AttackSourceComponent>(weapon->GetComponent<AttackSourceComponent>()),
			Handle<TrailPolygonComponent>(weapon->GetComponent<TrailPolygonComponent>()));
	}

	weapon->AddComponent<TwoBoneIKComponent>(
		"mixamorig:RightShoulder", "mixamorig:RightArm",
		"mixamorig:RightForeArm", "mixamorig:RightHand");

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

	auto* socket = weapon->AddComponent<AttachToSocketComponent>(handle);
	socket->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
		-90, DirectX::XMConvertToRadians(180), 0.0f));

	socket->SetLocalPositon({ 0.05f,0.1f,0.f });

	auto* skeleton = weapon->AddComponent<SkeletonComponent>();
	skeleton->SetModelData("Asset/Models/Character/Player/Tachi.gltf");
	weapon->AddComponent<ModelRenderComponent>();

	auto* collision = weapon->AddComponent<ColliderComponent>();
	// 常時enabled=trueのままだと、HurtBoxと重なっている間ずっと
	// CollisionEnterEventが発火し続け、ノックバックが際限なく積み増される
	// (実際に発生していた不具合)。攻撃発生フレーム(AttackActive)の間だけ
	// PlayerStatusController::SetWeaponHitBoxEnabled()経由で有効化する
	// 前提のため、生成直後はfalseにしておく。
	CollisionShapeEntry& hitBox = collision->AddBox(
		//"HitBox", Math::Vector3(0.1f, 0.1f, 0.75f), Math::Vector3(0.f, 0.f, 0.75f), ColliderCategory::HitBox);
		"HitBox", Math::Vector3(0.1f, 0.1f, 0.75f) * 3, Math::Vector3(0.f, 0.f, 0.75f), ColliderCategory::HitBox);
	hitBox.enabled = false;
	// 押し返し(物理応答)はせず、重なり検知(イベント)だけ行う。
	// isTrigger未設定のままだと通常のBump同様の押し返しが働いてしまう
	// (Enemy側のHurtBoxで実際に発生した不具合と同種)。以前このHitBoxに
	// 限り設定が漏れていたため、明示的に追加している。
	hitBox.isTrigger = true;
	collision->IgnoreCollisionWith(player);

	auto* attackSource = weapon->AddComponent<AttackSourceComponent>();
	// パリィ成立時、被弾側(この武器で攻撃された相手)が「攻撃者本体」の
	// PostureComponentを引き当てられるようにする(otherObjectは武器自体の
	// GameObjectであり、プレイヤー本体ではないため)。
	attackSource->ownerCharacter = Handle<GameObject>(player);

	weapon->AddComponent<WireFrameComponent>();

	// 攻撃の軌跡エフェクト。TrailPolygonComponentはデータ管理のみを担当し、
	// 実際の描画はPolygonRenderComponent(IPolygonRenderSourceを同じ
	// GameObject上から自動的に見つけて描画する側)が行うため、両方をセットで
	// 付ける必要がある(TrailPolygonEffectComponent.h冒頭コメント参照)。
	// 発生/停止のタイミングはPlayerStatusController::SetWeaponTrailEmitting()
	// 経由でStateAttack側から制御する(SetWeaponHitBoxEnabledと同じ窓)。
	weapon->AddComponent<PolygonRenderComponent>();
	weapon->AddComponent<TrailPolygonComponent>();

	return weapon;
}