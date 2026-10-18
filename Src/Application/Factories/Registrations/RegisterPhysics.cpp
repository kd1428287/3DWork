// 物理(当たり判定・重力・接地)と移動まわりのコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Definitions/Prefab/BitFlags.h"
#include "Application/Definitions/Physics/ColliderCategoryNames.h"
#include "Application/Definitions/Physics/ColliderParams.h"

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Movement/GravityComponent.h"
#include "Application/Components/Physics/Movement/VelocityComponent.h"
#include "Application/Components/Physics/Movement/MovementComponent.h"
#include "Application/Components/Physics/Movement/MovementResolverComponent.h"
#include "Application/Components/Physics/Sensors/GroundSensorComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerInputComponent.h"

using namespace Registration;

COMPONENT_PARAMS_DEFINE_TYPE(VelocityConfig, dampingPerSecond)
COMPONENT_PARAMS_DEFINE_TYPE(MovementConfig, speed)
COMPONENT_PARAMS_DEFINE_TYPE(GroundSensorConfig, footOffset, checkDistance)

// Colliderの組み立て(paramsの型はColliderParams.h)。
namespace
{
	void BuildCollider(BuildContext& ctx, const ColliderParams& p)
	{
		GameObject* ignoreTarget = p.ignoreParent ? RequireParent(ctx, "Collider.ignoreParent") : nullptr;

		auto* collider = ctx.Add<ColliderComponent>();
		if (p.wireFrame) AddWireFrameOnce(ctx.self);

		for (const ColliderShapeParams& s : p.shapes) {
			// collideMaskは既定ではレイヤーマトリクスの設定に任せる。
			const ColliderCategory category = ColliderCategoryFromNames(s.categoryMask);
			const ColliderCategory collideMask = s.useDefaultCollideMask
				? ColliderCategory::UseMatrixDefault
				: ColliderCategoryFromNames(s.collideMask);

			CollisionShapeEntry* entry = nullptr;
			if (s.shape == "Sphere") {
				entry = &collider->AddSphere(s.name, s.radius, s.offset, category, collideMask);
			}
			else if (s.shape == "Capsule") {
				entry = &collider->AddCapsule(s.name, s.radius, s.offset, s.capsuleEnd, category, collideMask);
			}
			else {
				entry = &collider->AddBox(s.name, s.halfExtents, s.offset, category, collideMask);
			}

			entry->enabled = s.enabled;
			entry->isTrigger = s.isTrigger;
			entry->isStatic = s.isStatic;
			entry->wantsStayEvent = s.wantsStayEvent;
		}

		collider->SetBodyShape(p.bodyShapeName);

		if (ignoreTarget) collider->IgnoreCollisionWith(ignoreTarget);
	}
}  // namespace

void RegisterPhysicsComponents(ComponentRegistry& registry)
{
	registry.AddWithParams<ColliderParams>("Collider", BuildCollider);
	registry.Add<GravityComponent>("Gravity");
	registry.Add<GroundSensorComponent>("GroundSensor");
	registry.Add<VelocityComponent>("Velocity");
	registry.Add<MovementResolverComponent>("MovementResolver");

	registry.AddWithParams<MovementConfig>("Movement", [](BuildContext& ctx, const MovementConfig& config) {
		auto* movement = ctx.Add<MovementComponent>();
		// 速度はscale=1基準。インプレースのアニメの歩幅に合わせて拡大率を掛ける(足滑り防止)。
		MovementConfig scaled = config;
		scaled.speed *= HorizontalScale(ctx);
		movement->SetConfig(scaled);
		// IMovementSourceはTAG_INTERFACESに無く、Awakeのタグ検索では拾えないため、PlayerInputを明示的に接続する。
		if (auto* input = ctx.self.GetComponent<PlayerInputComponent>()) movement->SetMovementSource(input);
		});
}