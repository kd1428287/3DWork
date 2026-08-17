#include "EnemyFactory.h"
#include "EnemyDefinition.h"


#include "../../Components/Character/Enemy/EnemyStatusController.h"
#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Movement/MovementComponent.h"
#include "../../Components/Movement/FacingDirectionComponent.h"
#include "../../Components/Collision/ColliderComponent.h"
#include "../../Components/Collision/CharacterCollisionDefaults.h"
#include "../../Components/Collision/GravityComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
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
	enemy->AddComponent<SkeletonComponent>()->SetModelData("Asset/Models/Character/Player/box.gltf");

	// EnemyStatusController::Start()内でMovementComponentへ
	// SetMovementSource(this)する。GetComponent<T>()はマップ参照なので、
	// AddComponentの順序には依存しない。
	enemy->AddComponent<EnemyStatusController>(def.patrolDistance);

	// 被弾判定(トリガー、攻撃判定専用)。collideMaskをHitBoxだけに
	// 絞っているため、これだけでは地形(Ground)を含めどのカテゴリとも
	// 物理的に判定しない(お互いのcollideMask/categoryMaskが噛み合う
	// 必要があるため。ColliderComponent::CanCollideWith参照)。
	ColliderComponent* collider = enemy->AddComponent<ColliderComponent>();
	collider->AddSphere("HurtBox", def.bodyRadius, {}, ColliderCategory::HurtBox, ColliderCategory::HitBox);

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
		Math::Vector3(0.0f, -kFootOffset + def.bodyRadius, 0.0f),
		Math::Vector3(0.0f, kFootOffset - def.bodyRadius, 0.0f),
		ColliderCategory::Bump);

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

	enemy->AddComponent<ModelRenderComponent>();
	enemy->AddComponent<FacingDirectionComponent>();

	return enemy;
}