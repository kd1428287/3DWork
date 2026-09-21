#include "ComponentRegistry.h"
#include "Application/Definitions/Physics/ColliderCategoryNames.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Definitions/Character/Player/PlayerDefinitionJson.h"
#include "Application/Definitions/Loaders/PlayerAttackTableLoader.h"
#include "Application/Definitions/Character/Common/CharacterCollisionDefaults.h"
#include "Application/Definitions/Character/Enemy/EnemyAIDataJson.h"

// ここでのみ実際のコンポーネントクラスに依存する。
#include "Application/Components/Core/TransformComponent.h"
#include "Application/Components/Core/AttachToSocketComponent.h"
#include "Application/Components/Core/BoneSocketComponent.h"

#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/WireFrameComponent.h"
#include "Application/Components/Graphics/Effect/SlashTrailComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Animation/RootMotionApplierComponent.h"
#include "Application/Components/Graphics/Animation/FacingDirectionComponent.h"
#include "Application/Components/Graphics/Animation/TwoBoneIKComponent.h"

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Movement/FollowCameraComponent.h"
#include "Application/Components/Physics/Movement/GravityComponent.h"
#include "Application/Components/Physics/Movement/VelocityComponent.h"
#include "Application/Components/Physics/Movement/MovementComponent.h"
#include "Application/Components/Physics/Movement/MovementResolverComponent.h"
#include "Application/Components/Physics/Sensors/GroundSensorComponent.h"

#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"
#include "Application/Components/GamePlay/Character/Common/AttackSourceComponent.h"
#include "Application/Components/GamePlay/Character/Common/HealthComponent.h"
#include "Application/Components/GamePlay/Character/Common/HitReactionComponent.h"
#include "Application/Components/GamePlay/Character/Common/PostureComponent.h"
#include "Application/Components/GamePlay/Character/Common/WeaponComponent.h"
#include "Application/Components/GamePlay/Character/Common/WeaponSetComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerAttackSelector.h"
#include "Application/Components/GamePlay/Character/Player/PlayerCombatMovementComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerFacingComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerInputComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerLockOnComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerMovementAnimationComponent.h"
#include "Application/Components/GamePlay/Character/Player/PlayerStatusController.h"
#include "Application/Components/GamePlay/Character/Enemy/Warrock/WarrockBehavior.h"


// TODO: 実際のヘッダパスに合わせて調整すること(未着手ファイルのため未確認)。
#include "Application/Components/GamePlay/Character/Enemy/EnemyAIController.h"
#include "Application/Components/GamePlay/Character/Enemy/LockOnTargetComponent.h"
#include "Application/Components/Graphics/UI/GamePlay/EnemyWorldGaugeComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 各コンポーネントのparams。キー名はメンバ名と同じで、未指定のキーは初期値になる。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

// 未知の軸名は先頭のYになる。
NLOHMANN_JSON_SERIALIZE_ENUM(RootMotionAxis, {
	{ RootMotionAxis::Y, "Y" },
	{ RootMotionAxis::X, "X" },
	{ RootMotionAxis::Z, "Z" },
	})

	// 他のファイルの同名の型とODR違反にならないよう、無名名前空間に入れる(マクロも同じ名前空間に置く)。
	namespace
{

	struct ModelRenderParams
	{
		std::string model;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModelRenderParams, model)

		struct ModelAnimatorParams
	{
		int          fps = 60;
		float        speedScale = 1.0f;
		std::string  rootMotionBone;
		RootMotionAxis rootMotionAxis = RootMotionAxis::Y;
		float        rootMotionAxisSign = -1.0f;
		float        rootMotionScale = 0.01f;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModelAnimatorParams,
		fps, speedScale, rootMotionBone, rootMotionAxis, rootMotionAxisSign, rootMotionScale)

		struct FollowCameraParams
	{
		Math::Vector3 offset = { 0.0f, -25.0f, 0.0f };
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FollowCameraParams, offset)

		// rotationSpeedが負の場合はFacingDirectionComponent自身の既定値のままにする
		// (SetRotationSpeedを呼ばない)。従来のRegister<T>("FacingDirection")と
		// 同じ挙動を維持しつつ、Enemy側で個別の速度を指定できるようにするため。
		struct FacingDirectionParams
	{
		float rotationSpeed = -1.0f;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FacingDirectionParams, rotationSpeed)

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
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ColliderShapeParams,
		name, shape, offset, radius, capsuleEnd, halfExtents,
		categoryMask, useDefaultCollideMask, collideMask, enabled, isTrigger, isStatic, wantsStayEvent)

		// wireFrameがtrueなら、デバッグ用のWireFrameも一緒に付ける(未追加の場合のみ)。
		// ignoreParentがtrueなら、親オブジェクトとの衝突を無視する(武器など)。
		struct ColliderParams
	{
		std::vector<ColliderShapeParams> shapes;
		bool                             wireFrame = true;
		bool                             ignoreParent = false;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ColliderParams, shapes, wireFrame, ignoreParent)

		struct VelocityParams
	{
		float dampingPerSecond = 0.05f;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(VelocityParams, dampingPerSecond)

		struct MovementParams
	{
		float speed = 1.0f;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MovementParams, speed)

		struct PostureParams
	{
		float max = 100.0f;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PostureParams, max)

		struct HealthParams
	{
		float max = 100.0f;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HealthParams, max)

		// 既定値は目線の高さ(体格の共通定数)。
		struct CameraTargetParams
	{
		Math::Vector3 offset = { 0.0f, CharacterCollisionDefaults::kEyeHeight, 0.0f };
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CameraTargetParams, offset)

		struct PlayerCombatMovementParams
	{
		float walkSpeed = 4.0f;
		float runSpeed = 8.0f;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayerCombatMovementParams, walkSpeed, runSpeed)

		struct PlayerStatusControllerParams
	{
		EvadeData evade;
		GuardData guard;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayerStatusControllerParams, evade, guard)

		struct PlayerAttackSelectorParams
	{
		std::string attackTablePath;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayerAttackSelectorParams, attackTablePath)

		// 親のボーンに追従するソケット用オブジェクトを、boneごとに生成する。
		struct BoneSocketsParams
	{
		std::vector<std::string> bones;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BoneSocketsParams, bones)

		// rotationEulerDegのx/y/zは、それぞれYaw/Pitch/Rollとして渡す(既存の挙動)。
		struct AttachToBoneParams
	{
		std::string   bone;
		Math::Vector3 position = { 0.0f, 0.0f, 0.0f };
		Math::Vector3 rotationEulerDeg = { 0.0f, 0.0f, 0.0f };
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AttachToBoneParams, bone, position, rotationEulerDeg)

		struct TwoBoneIKParams
	{
		std::string rootBone;
		std::string midBone;
		std::string tipParentBone;
		std::string tipBone;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TwoBoneIKParams, rootBone, midBone, tipParentBone, tipBone)

		struct SlashTrailParams
	{
		std::string   name = "Sword";
		Math::Vector3 base = { 0.0f, 0.0f, 0.0f };
		Math::Vector3 tip = { 0.0f, 0.0f, 0.0f };
		std::string   key;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SlashTrailParams, name, base, tip, key)

	// behavior名からIEnemyBehaviorを選ぶ。他の敵種を追加する場合はここに分岐を足す。
	// (BruteBehaviorはIEnemyBehaviorへまだ追従できていないため未対応 — IEnemyBehavior.h参照)
	struct EnemyAIParams
	{
		std::string behavior;
		EnemyAIData aiData;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EnemyAIParams, behavior, aiData)

}  // namespace

namespace
{
	GameObject* RequireParent(BuildContext& ctx, const char* who)
	{
		if (!ctx.parent) throw std::runtime_error(std::string(who) + " requires a parent object");
		return ctx.parent;
	}

	SkeletonComponent* RequireSkeleton(GameObject* obj, const char* who)
	{
		auto* skeleton = obj->GetComponent<SkeletonComponent>();
		if (!skeleton) throw std::runtime_error(std::string(who) + " requires SkeletonComponent on the parent");
		return skeleton;
	}

	// boneのボーンに追従するBoneSocketComponentだけを持つオブジェクトを生成する。
	GameObject* CreateBoneSocket(ObjectManager& objectManager, SkeletonComponent* skeleton, const std::string& bone)
	{
		GameObject* socket = objectManager.Instantiate(bone);
		Handle<SkeletonComponent> handle(skeleton);
		socket->AddComponent<BoneSocketComponent>(handle, bone);
		return socket;
	}

	void AddWireFrameOnce(GameObject& obj)
	{
		if (!obj.HasComponent<WireFrameComponent>()) obj.AddComponent<WireFrameComponent>();
	}

	void BuildCollider(BuildContext& ctx, const ColliderParams& p)
	{
		GameObject* ignoreTarget = p.ignoreParent ? RequireParent(ctx, "Collider.ignoreParent") : nullptr;

		auto* collider = ctx.self.AddComponent<ColliderComponent>();
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

		if (ignoreTarget) collider->IgnoreCollisionWith(ignoreTarget);
	}

	void BuildBoneSockets(BuildContext& ctx, const BoneSocketsParams& p)
	{
		SkeletonComponent* skeleton = RequireSkeleton(&ctx.self, "BoneSockets");
		for (const std::string& bone : p.bones) CreateBoneSocket(ctx.objectManager, skeleton, bone);
	}

	// 親のボーンにソケット用オブジェクトを作り、自分をそこへ取り付ける。
	void BuildAttachToBone(BuildContext& ctx, const AttachToBoneParams& p)
	{
		SkeletonComponent* skeleton = RequireSkeleton(RequireParent(ctx, "AttachToBone"), "AttachToBone");

		GameObject* socket = CreateBoneSocket(ctx.objectManager, skeleton, p.bone);
		Handle<TransformComponent> attachPoint(socket->GetComponent<BoneSocketComponent>());

		auto* attach = ctx.self.AddComponent<AttachToSocketComponent>(attachPoint);
		attach->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(p.rotationEulerDeg.x),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.y),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.z)));
		attach->SetLocalPositon(p.position);
	}

	// 親がPlayer/Enemyどちらかを見て、そちらへ武器として登録する。
	void BuildWeapon(BuildContext& ctx, const nlohmann::json&)
	{
		auto* weapon = ctx.self.AddComponent<WeaponComponent>();
		if (!ctx.parent) return;

		if (auto* controller = ctx.parent->GetComponent<PlayerStatusController>()) {
			controller->SetWeapon(Handle<WeaponComponent>(weapon));
		}
		else if (auto* ai = ctx.parent->GetComponent<EnemyAIController>()) {
			ai->SetWeapon(Handle<WeaponComponent>(weapon));
		}
	}

	// コンボ木は専用ファイルから読む。読み込みに失敗しても、空のテーブルでコンポーネントは追加する。
	void BuildPlayerAttackSelector(BuildContext& ctx, const PlayerAttackSelectorParams& p)
	{
		PlayerAttackTable table;
		if (!PlayerAttackTableLoader::LoadFromFile(p.attackTablePath, table)) {
			OutputDebugStringA(("PlayerAttackSelector: attack table load failed: " + p.attackTablePath + "\n").c_str());
		}
		ctx.self.AddComponent<PlayerAttackSelector>()->SetAttackTable(table);
	}

	// 頭上ゲージ用の子オブジェクトを生成する(UIフラグ+ターゲット紐付け)。
	void BuildWorldGauge(BuildContext& ctx, const nlohmann::json&)
	{
		GameObject* gauge = ctx.objectManager.Instantiate("enemy_gauge");
		if (!gauge) return;

		gauge->AddFlag(ObjectFlags::UI);
		gauge->AddComponent<EnemyWorldGaugeComponent>()->SetTarget(Handle<GameObject>(&ctx.self));
	}

	// behavior名からIEnemyBehaviorを選び、EnemyAIController(実行層は共通)を追加する。
	void BuildEnemyAI(BuildContext& ctx, const EnemyAIParams& p)
	{
		std::unique_ptr<IEnemyBehavior> behavior;
		if (p.behavior == "Warrock") {
			behavior = std::make_unique<WarrockBehavior>();
		}
		// 他の敵種を追加する場合はここに分岐を足す(BruteBehaviorは現状未対応)。

		ctx.self.AddComponent<EnemyAIController>(p.aiData, std::move(behavior));
	}
}

// 新しいコンポーネントは、ここに登録を足せばPrefab/マップのtypeから使える。
void RegisterAllComponents(ComponentRegistry& registry)
{
	// --- 見た目 ---
	registry.RegisterWithParams<ModelRenderParams>("ModelRender", [](BuildContext& ctx, const ModelRenderParams& p) {
		ctx.self.AddComponent<SkeletonComponent>()->SetModelData(p.model);
		ctx.self.AddComponent<ModelRenderComponent>();
		});

	registry.RegisterWithParams<ModelAnimatorParams>("ModelAnimator", [](BuildContext& ctx, const ModelAnimatorParams& p) {
		auto* animator = ctx.self.AddComponent<ModelAnimatorComponent>();
		animator->SetFPS(p.fps);
		animator->SetSpeedScale(p.speedScale);
		animator->SetRootMotionBoneName(p.rootMotionBone);
		animator->SetRootMotionForwardAxis(p.rootMotionAxis, p.rootMotionAxisSign);
		animator->SetRootMotionScale(p.rootMotionScale);
		});

	registry.Register<RootMotionApplierComponent>("RootMotionApplier");

	registry.RegisterWithParams<FacingDirectionParams>("FacingDirection", [](BuildContext& ctx, const FacingDirectionParams& p) {
		auto* facing = ctx.self.AddComponent<FacingDirectionComponent>();
		if (p.rotationSpeed >= 0.0f) facing->SetRotationSpeed(p.rotationSpeed);
		});

	registry.Register("WireFrame", [](BuildContext& ctx, const nlohmann::json&) { AddWireFrameOnce(ctx.self); });

	registry.RegisterWithParams<FollowCameraParams>("FollowCamera", [](BuildContext& ctx, const FollowCameraParams& p) {
		ctx.self.AddComponent<FollowCameraComponent>()->SetOffset(p.offset);
		});

	registry.RegisterWithParams<CameraTargetParams>("CameraTarget", [](BuildContext& ctx, const CameraTargetParams& p) {
		ctx.self.AddComponent<CameraTargetComponent>()->SetOffset(p.offset);
		});

	// --- 物理・移動 ---
	registry.RegisterWithParams<ColliderParams>("Collider", BuildCollider);
	registry.Register<GravityComponent>("Gravity");
	registry.Register<GroundSensorComponent>("GroundSensor");
	registry.Register<MovementResolverComponent>("MovementResolver");

	registry.RegisterWithParams<VelocityParams>("Velocity", [](BuildContext& ctx, const VelocityParams& p) {
		ctx.self.AddComponent<VelocityComponent>(p.dampingPerSecond);
		});

	registry.RegisterWithParams<MovementParams>("Movement", [](BuildContext& ctx, const MovementParams& p) {
		auto* movement = ctx.self.AddComponent<MovementComponent>(p.speed);
		// 先に追加されたPlayerInputを入力ソースとして明示的に接続する(旧PlayerFactoryと同じ)。
		if (auto* input = ctx.self.GetComponent<PlayerInputComponent>()) movement->SetMovementSource(input);
		});

	// --- 戦闘(共通) ---
	registry.Register<HitReactionComponent, HitReactionConfig>("HitReaction");
	registry.Register<WeaponSetComponent>("WeaponSet");

	registry.RegisterWithParams<PostureParams>("Posture", [](BuildContext& ctx, const PostureParams& p) {
		ctx.self.AddComponent<PostureComponent>(p.max);
		});

	registry.RegisterWithParams<HealthParams>("Health", [](BuildContext& ctx, const HealthParams& p) {
		ctx.self.AddComponent<HealthComponent>()->SetMax(p.max, true);
		});

	// --- 武器・ソケット ---
	registry.RegisterWithParams<BoneSocketsParams>("BoneSockets", BuildBoneSockets);
	registry.RegisterWithParams<AttachToBoneParams>("AttachToBone", BuildAttachToBone);
	registry.Register("Weapon", BuildWeapon);

	registry.Register("AttackSource", [](BuildContext& ctx, const nlohmann::json&) {
		GameObject* owner = RequireParent(ctx, "AttackSource");
		ctx.self.AddComponent<AttackSourceComponent>()->ownerCharacter = Handle<GameObject>(owner);
		});

	registry.RegisterWithParams<TwoBoneIKParams>("TwoBoneIK", [](BuildContext& ctx, const TwoBoneIKParams& p) {
		ctx.self.AddComponent<TwoBoneIKComponent>(p.rootBone, p.midBone, p.tipParentBone, p.tipBone);
		});

	registry.RegisterWithParams<SlashTrailParams>("SlashTrail", [](BuildContext& ctx, const SlashTrailParams& p) {
		auto* trail = ctx.self.AddComponent<SlashTrailComponent>(p.name.c_str());
		trail->SetBaseTip(p.base, p.tip);
		trail->SetKey(p.key.c_str());
		trail->StartEmit();
		});

	// --- Player ---
	registry.Register<PlayerInputComponent>("PlayerInput");
	registry.Register<PlayerLockOnComponent>("PlayerLockOn");
	registry.Register<PlayerFacingComponent>("PlayerFacing");

	registry.RegisterWithParams<PlayerMovementAnimationDefinition>("PlayerMovementAnimation",
		[](BuildContext& ctx, const PlayerMovementAnimationDefinition& p) {
			ctx.self.AddComponent<PlayerMovementAnimationComponent>()->SetMovementAnimations(p);
		});

	registry.RegisterWithParams<PlayerStatusControllerParams>("PlayerStatusController",
		[](BuildContext& ctx, const PlayerStatusControllerParams& p) {
			ctx.self.AddComponent<PlayerStatusController>()->SetEvadeAndGuardData(p.evade, p.guard);
		});

	registry.RegisterWithParams<PlayerCombatMovementParams>("PlayerCombatMovement",
		[](BuildContext& ctx, const PlayerCombatMovementParams& p) {
			ctx.self.AddComponent<PlayerCombatMovementComponent>()->SetMovementSpeeds(p.walkSpeed, p.runSpeed);
		});

	registry.RegisterWithParams<PlayerAttackSelectorParams>("PlayerAttackSelector", BuildPlayerAttackSelector);

	// --- Enemy ---
	// LockOnTargetComponent/EnemyWorldGaugeComponentは未提供のヘッダのため、
	// コンストラクタ/SetTargetのシグネチャは実際のヘッダに合わせて調整すること。
	registry.Register<LockOnTargetComponent>("LockOnTarget");
	registry.Register("WorldGauge", BuildWorldGauge);
	registry.RegisterWithParams<EnemyAIParams>("EnemyAI", BuildEnemyAI);
}
