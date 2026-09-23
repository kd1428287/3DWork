#include "ComponentRegistry.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Prefab/BitFlags.h"
#include "Application/Definitions/Physics/ColliderCategoryNames.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Definitions/Loaders/PlayerAttackTableLoader.h"
#include "Application/Definitions/Character/Common/CharacterCollisionDefaults.h"
#include "Application/Definitions/Character/Player/PlayerDefinitionJson.h"
#include "Application/Definitions/Character/Enemy/EnemyDefinitionJson.h"
#include "Application/Definitions/Loaders/EnemyAttackTableLoader.h"

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

#include "Application/Components/GamePlay/Character/Enemy/EnemyAIController.h"
#include "Application/Components/GamePlay/Character/Enemy/Warrock/WarrockBehavior.h"
#include "Application/Components/GamePlay/Character/Enemy/LockOnTargetComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンポーネントのConfigのJSON変換。キー名はメンバ名と同じで、未指定のキーは初期値になる。
// (Configの構造体は各コンポーネントのヘッダにある)
//
// COMPONENT_PARAMS_DEFINE_TYPEの説明・理由はComponentParamsOrderedJson.hのコメントを参照。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

// 未知の軸名は先頭のYになる。
NLOHMANN_JSON_SERIALIZE_ENUM(RootMotionAxis, {
	{ RootMotionAxis::Y, "Y" },
	{ RootMotionAxis::X, "X" },
	{ RootMotionAxis::Z, "Z" },
	})

	COMPONENT_PARAMS_DEFINE_TYPE(SkeletonConfig, model, animations)

	COMPONENT_PARAMS_DEFINE_TYPE(RootMotionConfig,
		boneName, unitScale, forwardAxis, forwardSign, rightAxis, rightSign, extractRotation, yawSign)
	COMPONENT_PARAMS_DEFINE_TYPE(ModelAnimatorConfig, fps, speedScale, blendDuration, rootMotion)
	COMPONENT_PARAMS_DEFINE_TYPE(FollowCameraConfig, offset, followPosition, followRotation)
	COMPONENT_PARAMS_DEFINE_TYPE(CameraTargetConfig, offset)
	COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailConfig, trailName, base, tip, key, emitOnStart)
	COMPONENT_PARAMS_DEFINE_TYPE(VelocityConfig, dampingPerSecond)
	COMPONENT_PARAMS_DEFINE_TYPE(MovementConfig, speed)
	COMPONENT_PARAMS_DEFINE_TYPE(FacingDirectionConfig, rotationSpeed, moveThreshold)
	COMPONENT_PARAMS_DEFINE_TYPE(GroundSensorConfig, footOffset, checkDistance)
	COMPONENT_PARAMS_DEFINE_TYPE(PostureConfig, max, lowerLimit, regenPerSecond, regenDelaySeconds)
	COMPONENT_PARAMS_DEFINE_TYPE(HealthConfig, max)
	COMPONENT_PARAMS_DEFINE_TYPE(HitReactionConfig, effectFlags, damageEffectName, parryEffectName, blockEffectName,
		cameraShakeIntensity, hitStopDelaySeconds, hitStopDurationSeconds, guardKnockbackPower, largeStaggerDuration)
	COMPONENT_PARAMS_DEFINE_TYPE(PlayerCombatMovementConfig, walkSpeed, runSpeed)
	COMPONENT_PARAMS_DEFINE_TYPE(PlayerStatusControllerConfig, evade, guard)

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 他のコンポーネントとの接続やファイル読み込みを伴う型のparams。
	// 他のファイルの同名の型とODR違反にならないよう、無名名前空間に入れる(マクロも同じ名前空間に置く)。
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
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
		bool                             wireFrame = true;
		bool                             ignoreParent = false;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(ColliderParams, shapes, wireFrame, ignoreParent)

		struct PlayerAttackSelectorParams
	{
		std::string attackTablePath;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(PlayerAttackSelectorParams, attackTablePath)

		// 親のボーンに追従するソケット用オブジェクトを、boneごとに生成する。
		struct BoneSocketsParams
	{
		std::vector<std::string> bones;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(BoneSocketsParams, bones)

		// rotationEulerDegのx/y/zは、それぞれYaw/Pitch/Rollとして渡す(既存の挙動)。
		struct AttachToBoneParams
	{
		std::string   bone;
		Math::Vector3 position = { 0.0f, 0.0f, 0.0f };
		Math::Vector3 rotationEulerDeg = { 0.0f, 0.0f, 0.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(AttachToBoneParams, bone, position, rotationEulerDeg)

		struct TwoBoneIKParams
	{
		std::string rootBone;
		std::string midBone;
		std::string tipParentBone;
		std::string tipBone;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(TwoBoneIKParams, rootBone, midBone, tipParentBone, tipBone)

		// Enemy1体分の生成パラメータ。dataは挙動のチューニング値(EnemyAIData)、
		// behaviorは実際の行動ロジック(IEnemyBehaviorの具象クラス)を選ぶキー。
		// attackTablePathを指定すると、data.attacks/gapCloserAttacksはそちらの内容で
		// 上書きされる(PlayerAttackSelector.attackTablePathと同じ考え方。
		// 未指定ならdataに直接書かれたattacks/gapCloserAttacksをそのまま使う)。
		// 対応する具象クラスが増えたら、下のCreateEnemyBehavior()に足すこと。
		struct EnemyAIControllerParams
	{
		std::string behavior;
		std::string attackTablePath;
		EnemyAIData data;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(EnemyAIControllerParams, behavior, attackTablePath, data)

		std::unique_ptr<IEnemyBehavior> CreateEnemyBehavior(const std::string& behavior)
	{
		if (behavior == "Warrock") return std::make_unique<WarrockBehavior>();
		throw std::runtime_error("EnemyAIController: unknown behavior \"" + behavior + "\"");
	}

	// EnemyAIControllerはGameObject*/EnemyAIData/IEnemyBehaviorの3つをコンストラクタで
	// 要求しSetConfig()も持たないため、Add<T>()には乗らない(EnemyAIController.h冒頭コメント参照)
	void BuildEnemyAIController(BuildContext& ctx, const EnemyAIControllerParams& p)
	{
		EnemyAIData data = p.data;

		if (!p.attackTablePath.empty()) {
			EnemyAttackTable table;
			if (EnemyAttackTableLoader::LoadFromFile(p.attackTablePath, table)) {
				data.attacks = std::move(table.attacks);
				data.gapCloserAttacks = std::move(table.gapCloserAttacks);
			}
			else {
				OutputDebugStringA(("EnemyAIController: attack table load failed: " + p.attackTablePath + "\n").c_str());
			}
		}

		ctx.Add<EnemyAIController>(data, CreateEnemyBehavior(p.behavior));
	}

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

		auto* attach = ctx.Add<AttachToSocketComponent>(attachPoint);
		attach->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(p.rotationEulerDeg.x),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.y),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.z)));
		attach->SetLocalPositon(p.position);
	}

	// 親がPlayerStatusController/EnemyAIControllerのいずれかを持つ場合は、自分を武器として登録する。
	// (両者ともPlayer同様の設計でWeaponSetComponentへ委譲するSetWeapon()を持つ。
	// EnemyAIController.h冒頭コメント参照)
	void BuildWeapon(BuildContext& ctx, const nlohmann::json&)
	{
		auto* weapon = ctx.Add<WeaponComponent>();
		if (!ctx.parent) return;

		if (auto* controller = ctx.parent->GetComponent<PlayerStatusController>()) {
			controller->SetWeapon(Handle<WeaponComponent>(weapon));
		}
		else if (auto* enemy = ctx.parent->GetComponent<EnemyAIController>()) {
			enemy->SetWeapon(Handle<WeaponComponent>(weapon));
		}
	}

	// コンボ木は専用ファイルから読む。読み込みに失敗しても、空のテーブルでコンポーネントは追加する。
	void BuildPlayerAttackSelector(BuildContext& ctx, const PlayerAttackSelectorParams& p)
	{
		PlayerAttackTable table;
		if (!PlayerAttackTableLoader::LoadFromFile(p.attackTablePath, table)) {
			OutputDebugStringA(("PlayerAttackSelector: attack table load failed: " + p.attackTablePath + "\n").c_str());
		}
		ctx.Add<PlayerAttackSelector>()->SetAttackTable(table);
	}

}  // namespace

// 新しいコンポーネントは、ここに登録を足せばPrefab/マップのtypeから使える。
//   Add<T>          : T::Config(と SetConfig)を持つ型はparamsを自動で読む。持たない型はそのまま追加する
//   AddWithParams<P>: 他のコンポーネントとの接続などが要る型(paramsは型付きで受け取る)
//   AddRaw          : paramsを直接扱う型(関数ポインタのみ)
void RegisterAllComponents(ComponentRegistry& registry)
{
	// --- 見た目 ---
	registry.AddWithParams<SkeletonConfig>("ModelRender", [](BuildContext& ctx, const SkeletonConfig& config) {
		auto* skeleton = ctx.Add<SkeletonComponent>();
		skeleton->SetConfig(config);
		ctx.Add<ModelRenderComponent>();
		});
	registry.Add<ModelAnimatorComponent>("ModelAnimator");
	registry.Add<RootMotionApplierComponent>("RootMotionApplier");
	registry.Add<FacingDirectionComponent>("FacingDirection");
	registry.Add<FollowCameraComponent>("FollowCamera");
	registry.Add<CameraTargetComponent>("CameraTarget");
	registry.Add<SlashTrailComponent>("SlashTrail");
	registry.AddRaw("WireFrame", [](BuildContext& ctx, const nlohmann::json&) { AddWireFrameOnce(ctx.self); });

	// --- 物理・移動 ---
	registry.AddWithParams<ColliderParams>("Collider", BuildCollider);
	registry.Add<GravityComponent>("Gravity");
	registry.Add<GroundSensorComponent>("GroundSensor");
	registry.Add<VelocityComponent>("Velocity");
	registry.Add<MovementResolverComponent>("MovementResolver");

	registry.AddWithParams<MovementConfig>("Movement", [](BuildContext& ctx, const MovementConfig& config) {
		auto* movement = ctx.Add<MovementComponent>();
		movement->SetConfig(config);
		// IMovementSourceはTAG_INTERFACESに無く、Awakeのタグ検索では拾えないため、PlayerInputを明示的に接続する。
		if (auto* input = ctx.self.GetComponent<PlayerInputComponent>()) movement->SetMovementSource(input);
		});

	// --- 戦闘(共通) ---
	registry.Add<HitReactionComponent>("HitReaction");
	registry.Add<WeaponSetComponent>("WeaponSet");
	registry.Add<PostureComponent>("Posture");
	registry.Add<HealthComponent>("Health");

	// --- 武器・ソケット ---
	registry.AddWithParams<BoneSocketsParams>("BoneSockets", BuildBoneSockets);
	registry.AddWithParams<AttachToBoneParams>("AttachToBone", BuildAttachToBone);
	registry.AddRaw("Weapon", BuildWeapon);

	registry.AddRaw("AttackSource", [](BuildContext& ctx, const nlohmann::json&) {
		GameObject* owner = RequireParent(ctx, "AttackSource");
		ctx.Add<AttackSourceComponent>()->ownerCharacter = Handle<GameObject>(owner);
		});

	registry.AddWithParams<TwoBoneIKParams>("TwoBoneIK", [](BuildContext& ctx, const TwoBoneIKParams& p) {
		ctx.Add<TwoBoneIKComponent>(p.rootBone, p.midBone, p.tipParentBone, p.tipBone);
		});

	// --- Player ---
	registry.Add<PlayerInputComponent>("PlayerInput");
	registry.Add<PlayerLockOnComponent>("PlayerLockOn");
	registry.Add<PlayerFacingComponent>("PlayerFacing");
	registry.Add<PlayerMovementAnimationComponent>("PlayerMovementAnimation");
	registry.Add<PlayerStatusController>("PlayerStatusController");
	registry.Add<PlayerCombatMovementComponent>("PlayerCombatMovement");
	registry.AddWithParams<PlayerAttackSelectorParams>("PlayerAttackSelector", BuildPlayerAttackSelector);

	// --- Enemy ---
	registry.AddWithParams<EnemyAIControllerParams>("EnemyAIController", BuildEnemyAIController);
	registry.Add<LockOnTargetComponent>("LockOnTarget");
}