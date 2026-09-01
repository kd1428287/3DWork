#include "EnemyFactory.h"

#include "../../Components/Character/Enemy/EnemyDefinition.h"
#include "../../Components/Character/Enemy/IEnemyAIController.h"
#include "../../Components/Character/Enemy/EnemyAIController.h"
#include "../../Components/Character/Enemy/Warrock/WarrockAIController.h"
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
		//transform->SetScale({ 2.f,2.f,2.f });
		enemy->AddComponent<MovementComponent>(def.moveSpeed);
	}

	// --- 見た目(スケルトン+モデル描画) --------------------------------
	// 武器をボーンソケット経由で手に追従させるため、SkeletonComponentの
	// ポインタを呼び出し側(BuildEnemy)へ返す(武器ソケット生成時に使う)。
	SkeletonComponent* AttachVisuals(GameObject* enemy, const EnemyDefinition& def)
	{
		// PlayerFactory::AttachVisuals()と同じモデル(Walking.gltf、mixamoリグ)を
		// 使うようにした。武器をボーンソケット経由で手に追従させるため、
		// 以前のbox.gltf(単純な箱、ボーン無し)では対応するボーンが見つからず
		// 単位行列にフォールバックしてしまい、追従が成立しなかったため。
		//
		// 注意: モデルをbox.gltfからWalking.gltf(人型)へ変更したことで、
		// これまでワイヤーフレームを見ながら調整していたHurtBox/Bodyの寸法が
		// 新しい見た目のシルエットと合わなくなっている可能性がある。
		// 一度ワイヤーフレーム表示で確認し、必要なら再調整すること。
		SkeletonComponent* skeleton = enemy->AddComponent<SkeletonComponent>();
		skeleton->SetModelData(def.modelPath);
		enemy->AddComponent<ModelRenderComponent>();
		return skeleton;
	}

	// --- 意思決定・実行(AIController) ----------------------------------
	// def.type(EnemyType)を見て、アタッチするAIControllerの型を切り替える。
	// 以前はdef.typeに応じてBrute/BossStatusController(継承ベース)を
	// 出し分けていたが、その実行層自体を廃止しBTへ全面移行したため、
	// EnemyType自体を一度廃止していた。今回、Warrockのようにパラメータ
	// だけでなくロジック(BuildTree()構造)自体が異なる敵種を追加した
	// ため、「どのAIControllerをアタッチするか」を選ぶ用途に限定して
	// EnemyTypeを復活させている(EnemyDefinition.h冒頭コメント参照)。
	// 戻り値をIEnemyAIController*にしているのは、EnemyAIControllerと
	// WarrockAIControllerが継承関係を持たない完全に独立した実装のため
	// (両者が共通で実装する最小限のインターフェース。
	//  IEnemyAIController.h参照)。
	IEnemyAIController* CreateAIController(GameObject* enemy, const EnemyDefinition& def)
	{
		switch (def.type) {
		case EnemyType::Warrock:
			return enemy->AddComponent<WarrockAIController>(def.aiData);
		case EnemyType::Brute:
		default:
			return enemy->AddComponent<EnemyAIController>(def.aiData);
		}
	}

	// --- 当たり判定(HurtBox+Body) -------------------------------------
	void CreateColliders(GameObject* enemy, const EnemyDefinition& def)
	{
		// 被弾判定(トリガー、攻撃判定専用)。collideMaskをHitBoxだけに
		// 絞っているため、これだけでは地形(Ground)を含めどのカテゴリとも
		// 物理的に判定しない(お互いのcollideMask/categoryMaskが噛み合う
		// 必要があるため。ColliderComponent::CanCollideWith参照)。
		ColliderComponent* collider = enemy->AddComponent<ColliderComponent>();
		CollisionShapeEntry& hurtBox = collider->AddCapsule("HurtBox", def.bodyRadius,
			Math::Vector3(0.0f, def.bodyRadius, 0.0f),
			Math::Vector3(0.0f, (CharacterCollisionDefaults::kFootOffset * 2) - def.bodyRadius, 0.0f),
			ColliderCategory::HurtBox, ColliderCategory::HitBox);
		// 押し返し(物理応答)はせず、重なり検知(イベント)だけ行う。
		// isTrigger未設定のままだと通常のBump同様の押し返しが働いてしまい、
		// ノックバック方向とは無関係にHitBox/HurtBoxの重なり解消で位置が
		// 押し出されてしまう不具合があった(実際に発生した不具合)。
		hurtBox.isTrigger = true;

		// 物理的な「胴体」形状。地形・壁など(Bump/Ground)と実際に押し合い、
		// GravityComponentによる落下を地面で受け止めるのはこちらの役目。
		// HurtBoxとは別の形状として持たせているのは、「攻撃を受ける範囲」と
		// 「物理的に押し返される範囲」は本来別の概念であり、将来的に
		// 「攻撃判定は少し大きいが体は細い」といった調整を独立にできるように
		// するため。isTrigger=false(デフォルト)なので押し返しが有効になる。
		//
		// 下端をGroundSensorComponentのfootOffsetと一致させている点が重要。
		// footOffsetは「Transform原点から足裏までの距離」を表すため、Bodyの
		// 下端がここまで届いていないと、Bodyが地面に乗って静止した高さと、
		// GroundSensorComponentが接地レイを飛ばす基準高さがズレる。ズレると
		// 接地レイの発射点が地面の内部/下から始まる形になり、「発射点がAABB
		// 内部から始まるケースは非対応」(GroundSensorComponent.h参照)という
		// 制約によって永遠に接地を検出できず、重力が際限なく積み上がって
		// 最終的に地面を貫通する(実際に発生していた不具合の根本原因)。
		// 生の数値ではなくCharacterCollisionDefaults::kFootOffsetを直接参照
		// することで、GroundSensorComponent側のデフォルト値とここが将来
		// 食い違わないようにしている(詳細はCharacterCollisionDefaults.h参照)。
		using CharacterCollisionDefaults::kFootOffset;
		collider->AddCapsule("Body", def.bodyRadius,
			Math::Vector3(0.0f, kFootOffset - def.bodyRadius, 0.0f),
			Math::Vector3(0.0f, kFootOffset + def.bodyRadius, 0.0f),
			ColliderCategory::Bump);
	}

	// --- 物理(重力・速度・接地判定) -----------------------------------
	void CreatePhysicsComponents(GameObject* enemy)
	{
		// GravityComponentをVelocityComponentより先にAddComponentしている。
		// 「同名コンポーネントは追加順」で毎フレームUpdate()が呼ばれるため、
		// この順序が「着地の瞬間、今フレームの速度クリアが位置反映より
		// 先に間に合うかどうか」を左右する。VelocityComponentが先だと、
		// 前フレームのPostUpdateで接地が確定していても、今フレームの
		// VelocityComponent::Updateが「まだクリアされていない古い下向き
		// 速度」を先に位置へ反映してしまい、着地の瞬間に地面へめり込む→
		// CollisionSystemに押し戻される、を繰り返す不安定な挙動(バウンド)
		// の原因になる。GravityComponentを先にすることで、着地判定に
		// 基づく速度クリアが同じフレーム内のVelocityComponent::Updateより
		// 前に確定するようにしている。
		enemy->AddComponent<GravityComponent>();
		enemy->AddComponent<VelocityComponent>();
		enemy->AddComponent<GroundSensorComponent>();
	}

	// --- 戦闘補助(向き・体幹・HP・ロックオン・アニメーター) --------------
	void CreateCombatSupportComponents(GameObject* enemy)
	{
		enemy->AddComponent<FacingDirectionComponent>();
		//enemy->AddComponent<WireFrameComponent>();

		// 体幹(パリィ/ガードの削り合い)管理用。
		enemy->AddComponent<PostureComponent>();

		// HP管理用。数値の詳細(最大値等)はHealthComponentのデフォルト値
		// のまま、別途調整する。
		enemy->AddComponent<HealthComponent>();
		enemy->AddComponent<LockOnTargetComponent>();

		//enemy->AddComponent<EnemyHPBarComponent>();

		enemy->AddComponent<ModelAnimatorComponent>()->SetFPS(60);
	}

	// --- 武器のソケット生成 --------------------------------------------
	// PlayerFactory::CreateSocket()と同じ考え方(BoneSocketComponentで
	// スケルトンの特定ボーンに追従させる)。EnemyもPlayerと同じ
	// Walking.gltf(mixamoリグ)を使うようになったため、同じボーン名
	// ("mixamorig:RightHand")がそのまま使える。
	GameObject* CreateWeaponSocket(ObjectManager& objectManager, Handle<SkeletonComponent>& skeletonHandle) {
		GameObject* socket = objectManager.Instantiate("enemy_weapon_socket");
		if (!socket) return nullptr;
		socket->AddComponent<BoneSocketComponent>(skeletonHandle, "mixamorig:RightHand");
		return socket;
	}

	// --- 武器本体の生成 --------------------------------------------------
	// PlayerFactory::CreateWeapon()とほぼ同一の構成(見た目のモデルは
	// 仮のbox.gltfを共用)。IgnoreCollisionWith(enemy)で自分自身とは
	// 判定しないようにしている点もPlayer側と同じ。
	GameObject* CreateWeapon(ObjectManager& objectManager, GameObject* enemy, Handle<TransformComponent>& attachPoint) {
		GameObject* weapon = objectManager.Instantiate("enemy_weapon");
		if (!weapon) return nullptr;

		auto* transform = weapon->AddComponent<TransformComponent>();
		transform->SetScale({ 0.5f, 0.5f, 0.5f });

		weapon->AddComponent<AttachToSocketComponent>(attachPoint);

		//auto* skeleton = weapon->AddComponent<SkeletonComponent>();
		//skeleton->SetModelData("Asset/Models/Character/Player/Tachi.gltf");
		//weapon->AddComponent<ModelRenderComponent>();

		auto* collision = weapon->AddComponent<ColliderComponent>();
		// 常時enabled=trueのままだと、HurtBoxと重なっている間ずっと
		// CollisionEnterEventが発火し続けてしまう(Player側で実際に発生した
		// 不具合と同じ)。EnemyStatusController::UpdateAttackTimer()が
		// 一定間隔でだけ有効化する前提のため、生成直後はfalseにしておく。
		/*CollisionShapeEntry& hitBox = collision->AddBox(
			"HitBox", Math::Vector3(1.f, 1.f, 1.f), Math::Vector3(0.f, 1.f, 0.f), ColliderCategory::HitBox);*/
		CollisionShapeEntry& hitBox = collision->AddBox(
			"HitBox", Math::Vector3(1.f, 1.f, 0.1f), Math::Vector3(1.f, 1.f, 0.f), ColliderCategory::HitBox);
		hitBox.enabled = false;
		// 押し返し(物理応答)はせず、重なり検知(イベント)だけ行う。
		// EnemyFactory側のHurtBoxで既に発生していたのと同種の不具合
		// (isTrigger未設定のままだと物理的な押し返しが働いてしまう)を
		// 未然に防ぐため、生成時点で明示しておく。
		hitBox.isTrigger = true;
		collision->IgnoreCollisionWith(enemy);

		auto* attackSource = weapon->AddComponent<AttackSourceComponent>();
		// 専用の武器GameObjectを持つようになったので、パリィ成立時に
		// 攻撃者本体のPostureComponentを引き当てられるよう、Player側と
		// 同様にownerCharacterへ本体(enemy)を登録しておく。
		attackSource->ownerCharacter = Handle<GameObject>(enemy);

		weapon->AddComponent<WireFrameComponent>();

		return weapon;
	}

	// --- 武器の生成・取り付け・道連れ登録 ------------------------------
	// IEnemyAIController経由で扱うため、EnemyAIController/
	// WarrockAIControllerのどちらがアタッチされていても同じコードで
	// 武器を取り付けられる。RegisterOwnedObject()はデフォルトで
	// 空実装のため、死亡処理を持たない敵種(Warrock等)ではここで
	// 登録しても実質何も起きない(IEnemyAIController.h参照)。
	void AttachWeaponAndRegister(ObjectManager& objectManager, GameObject* enemy,
		IEnemyAIController* ai, Handle<SkeletonComponent>& skeletonHandle)
	{
		GameObject* weaponSocket = CreateWeaponSocket(objectManager, skeletonHandle);
		Handle<TransformComponent> weaponAttachPoint(weaponSocket->GetComponent<BoneSocketComponent>());

		GameObject* weapon = CreateWeapon(objectManager, enemy, weaponAttachPoint);
		if (weapon != nullptr) {
			ai->SetWeapon(
				Handle<ColliderComponent>(weapon->GetComponent<ColliderComponent>()),
				Handle<AttackSourceComponent>(weapon->GetComponent<AttackSourceComponent>()));
		}

		// 武器・武器ソケットは敵本体とは別のGameObjectのため、敵が死亡して
		// 消滅する際に道連れで破棄されるよう明示的に登録しておく
		// (登録しないと、当たり判定は無効化されても見た目上ワールドに
		//  浮いたまま残り続けてしまう。詳細はEnemyAIController::
		//  RequestDespawn()参照)。
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
	IEnemyAIController* ai = CreateAIController(enemy, def);
	CreateColliders(enemy, def);
	CreatePhysicsComponents(enemy);
	CreateCombatSupportComponents(enemy);

	Handle<SkeletonComponent> skeletonHandle(skeleton);
	AttachWeaponAndRegister(objectManager, enemy, ai, skeletonHandle);

	return enemy;
}