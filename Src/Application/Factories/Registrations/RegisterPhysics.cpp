// 物理(当たり判定・重力・接地)と移動まわりのコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Definitions/Prefab/BitFlags.h"
#include "Application/Definitions/Physics/ColliderCategoryNames.h"

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

// 接続を伴う型のparams。ODR違反を避けるため、マクロも含めて無名名前空間に入れる。
namespace
{
	// Colliderの1形状分。shapeは"Sphere"/"Capsule"/"Box"(未知の値はBox)。
	// Capsuleはoffsetが始点、capsuleEndが終点。マスクはColliderCategoryNames.hの名前の配列で指定する。
	struct ColliderShapeParams
	{
		std::string name = "shape";
		std::string shape = "Box";

		Math::Vector3 offset = { 0.0f, 0.0f, 0.0f };
		float         radius = 0.5f;
		Math::Vector3 capsuleEnd = { 0.0f, 1.0f, 0.0f };
		Math::Vector3 halfExtents = { 0.5f, 0.5f, 0.5f };

		std::vector<std::string> categoryMask = { "Bump" };
		bool                     useDefaultCollideMask = true;
		std::vector<std::string> collideMask;

		bool enabled = true;
		bool isTrigger = false;
		bool isStatic = false;
		bool wantsStayEvent = false;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(ColliderShapeParams,
		name, shape, offset, radius, capsuleEnd, halfExtents,
		categoryMask, useDefaultCollideMask, collideMask, enabled, isTrigger, isStatic, wantsStayEvent)

	// wireFrameがtrueなら、デバッグ用のWireFrameも一緒に付ける(未追加の場合のみ)。
	// ignoreParentがtrueなら、親オブジェクトとの衝突を無視する(武器など)。
	struct ColliderParams
	{
		std::vector<ColliderShapeParams> shapes;
		std::string						 bodyShapeName;
		bool                             wireFrame = false;
		bool                             ignoreParent = false;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(ColliderParams, shapes, bodyShapeName, wireFrame, ignoreParent)

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
