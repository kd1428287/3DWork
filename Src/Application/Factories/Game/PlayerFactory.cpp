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
#include "../../Components/Effect/TrailPolygonEffectComponent.h"

namespace
{
	// 見た目・描画・アニメーション関連のセットアップ。
	// SkeletonComponentはソケット生成側でHandleとして必要になるため返す。
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

	// 物理・当たり判定関連のセットアップ(重力・速度・接地判定・押し返し用Body)。
	// 呼び出し側がBodyの形状を直接いじることは今のところ無いが、将来の拡張
	// (装備品でのBody差し替え等)を見据えて一応返しておく。
	ColliderComponent* AttachPhysics(GameObject* player, const PlayerCombatStatsDefinition& combatStats,
		const std::vector<CapsuleColliderDefinition>& colliderDefs, const IKChainDefinition& rightArmIK)
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

		// HP管理用。
		auto* health = player->AddComponent<HealthComponent>();
		health->SetMax(combatStats.maxHealth, true);

		//player->AddComponent<PlayerStatusUIComponent>();

		player->AddComponent<PlayerLockOnComponent>();

		player->AddComponent<TwoBoneIKComponent>(
			rightArmIK.rootBone, rightArmIK.midBone, rightArmIK.tipParentBone, rightArmIK.tipBone);

		// カプセルの並び(Y方向オフセット)はCharacterCollisionDefaults::kFootOffset
		// と一致している必要がある点が重要。Bodyの下端がfootOffsetまで届いて
		// いないと、Bodyが地面に乗って静止した高さとGroundSensorComponentの
		// 接地レイの基準高さがズレる(詳細はCharacterCollisionDefaults.h、
		// およびEnemyFactory::BuildEnemy側の同種の修正コメント参照)。
		// この整合性はデータ側(Player.json)の責任であり、Factory側では
		// 検証していない点に注意(値を変えるときはfootOffsetと合わせること)。
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

	// 入力・移動関連のセットアップ。
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
	// ObjectManagerがInstantiate時に内部の所有権コンテナ(std::vector<
	// std::unique_ptr<GameObject>>等)へ格納し、その生ポインタを返して
	// くれている想定。
	GameObject* player = objectManager.Instantiate("player");
	if (!player) return nullptr;

	SkeletonComponent* skeleton = AttachVisuals(player, definition.visuals);
	AttachPhysics(player, definition.combatStats, definition.colliders, definition.rightArmIK);
	AttachMovement(player, definition.walkSpeed);

	player->AddComponent<PlayerStatusController>();
	player->AddComponent<CameraTargetComponent>()->SetLocalPosition({ 0.f,0.5f,0.f });

	// --- 腕・武器のソケット生成 -----------------------------------------
	// 武器の取り付け先(右手)以外のソケットは、現時点では取り付け先として
	// 使っていないが、将来的な装備拡張(盾・エフェクト等)や、SkeletonComponent
	// のボーン構成確認用として生成だけ済ませておく。対象ボーンの一覧は
	// definition.auxiliarySocketBonesに集約されているため、増減はデータ側で行う。
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

GameObject* PlayerFactory::CreateWeapon(ObjectManager& objectManager, GameObject* player, Handle<TransformComponent>& handle,
	const WeaponDefinition& weaponDefinition, const IKChainDefinition& ikChain)
{
	GameObject* weapon = objectManager.Instantiate("weapon");
	if (!weapon) return nullptr;

	auto* transform = weapon->AddComponent<TransformComponent>();

	auto* socket = weapon->AddComponent<AttachToSocketComponent>(handle);
	// 注意: Yawだけ度数のまま、Pitchのみ XMConvertToRadians を通す非対称な
	// 呼び出しは既存実装からそのまま踏襲している(データ化前と見た目を
	// 変えないため)。単位を揃えたい場合は挙動が変わるので別途確認すること。
	socket->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
		weaponDefinition.socketLocalEulerRotationDeg.x,
		DirectX::XMConvertToRadians(weaponDefinition.socketLocalEulerRotationDeg.y),
		weaponDefinition.socketLocalEulerRotationDeg.z));

	socket->SetLocalPositon(weaponDefinition.socketLocalPosition);

	auto* skeleton = weapon->AddComponent<SkeletonComponent>();
	skeleton->SetModelData(weaponDefinition.modelPath);
	weapon->AddComponent<ModelRenderComponent>();

	auto* collision = weapon->AddComponent<ColliderComponent>();
	// 常時enabled=trueのままだと、HurtBoxと重なっている間ずっと
	// CollisionEnterEventが発火し続け、ノックバックが際限なく積み増される
	// (実際に発生していた不具合)。攻撃発生フレーム(AttackActive)の間だけ
	// PlayerStatusController::SetWeaponHitBoxEnabled()経由で有効化する
	// 前提のため、生成直後はfalseにしておく。この初期状態はデータ化せず
	// Factory側の不変条件として固定している。
	CollisionShapeEntry& hitBox = collision->AddBox(
		"HitBox", weaponDefinition.hitBox.halfExtents, weaponDefinition.hitBox.offset, ColliderCategory::HitBox);
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
	//trail->SetPattern(KdTrailPolygon::Trail_Pattern::eBillboard);
	trail->SetPattern(KdTrailPolygon::Trail_Pattern::eVertices);
	trail->SetBaseTip({ 0,0,0 }, weaponDefinition.hitBox.offset);

	return weapon;
}
