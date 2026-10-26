// 戦闘(共通)と、武器・ボーンソケットまわりのコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Definitions/Character/Common/CharacterCollisionDefaults.h"

#include "Application/Components/Core/TransformComponent.h"
#include "Application/Components/Core/AttachToSocketComponent.h"
#include "Application/Components/Core/BoneSocketComponent.h"
#include "Application/Components/Core/AttachBoneToBoneComponent.h"

#include "Application/Components/Graphics/Animation/TwoBoneIKComponent.h"

#include "Application/Components/GamePlay/Character/Common/AttackSourceComponent.h"
#include "Application/Components/GamePlay/Character/Common/HealthComponent.h"
#include "Application/Components/GamePlay/Character/Common/LifeComponent.h"
#include "Application/Components/GamePlay/Character/Common/HitReactionComponent.h"
#include "Application/Components/GamePlay/Character/Common/PostureComponent.h"
#include "Application/Components/GamePlay/Character/Common/WeaponComponent.h"
#include "Application/Components/GamePlay/Character/Common/WeaponSetComponent.h"

using namespace Registration;

COMPONENT_PARAMS_DEFINE_TYPE(PostureConfig, max, lowerLimit, regenPerSecond, regenDelaySeconds)
COMPONENT_PARAMS_DEFINE_TYPE(HealthConfig, max)
COMPONENT_PARAMS_DEFINE_TYPE(LifeConfig, max)

// 接続を伴う型のparams。ODR違反を避けるため、マクロも含めて無名名前空間に入れる。
namespace
{
	// 武器(攻撃部位)を登録するスロット名。未指定ならPlayer同様"Main"扱い
	// (既存Prefabは変更不要)。敵が複数の攻撃部位を持つ場合に指定する。
	struct WeaponParams
	{
		std::string slot = "Main";
	};
	COMPONENT_PARAMS_DEFINE_TYPE(WeaponParams, slot)

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

		// 子のchildBoneを、親のparentBoneに重ねる。オフセットはAttachToBoneと同じ。
		struct AttachBoneToBoneParams
	{
		std::string   parentBone;
		std::string   childBone;
		Math::Vector3 position = { 0.0f, 0.0f, 0.0f };
		Math::Vector3 rotationEulerDeg = { 0.0f, 0.0f, 0.0f };
	};
	COMPONENT_PARAMS_DEFINE_TYPE(AttachBoneToBoneParams, parentBone, childBone, position, rotationEulerDeg)

		struct TwoBoneIKParams
	{
		std::string rootBone;
		std::string midBone;
		std::string tipParentBone;
		std::string tipBone;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(TwoBoneIKParams, rootBone, midBone, tipParentBone, tipBone)

		void BuildBoneSockets(BuildContext& ctx, const BoneSocketsParams& p)
	{
		SkeletonComponent* skeleton = RequireSkeleton(&ctx.self, "BoneSockets");
		for (const std::string& bone : p.bones) CreateBoneSocket(ctx.objectManager, skeleton, bone);
	}

	// 親のボーンにソケット用オブジェクトを作り、自分をそこへ取り付ける。
	void BuildAttachToBone(BuildContext& ctx, const AttachToBoneParams& p)
	{
		GameObject* parent = RequireParent(ctx, "AttachToBone");
		SkeletonComponent* skeleton = RequireSkeleton(parent, "AttachToBone");

		GameObject* socket = CreateBoneSocket(ctx.objectManager, skeleton, p.bone);
		Handle<TransformComponent> attachPoint(socket->GetComponent<BoneSocketComponent>());

		auto* attach = ctx.Add<AttachToSocketComponent>(attachPoint);
		attach->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(p.rotationEulerDeg.x),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.y),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.z)));
		attach->SetLocalPositon(p.position);
		// 持ち主(親)のscaleに追従させる
		attach->SetScaleSource(Handle<TransformComponent>(parent->GetComponent<TransformComponent>()));
	}

	// 親のボーンにソケットを作り、自分のchildBoneをそこへ重ねる(自分にSkeletonが必要)。
	void BuildAttachBoneToBone(BuildContext& ctx, const AttachBoneToBoneParams& p)
	{
		GameObject* parent = RequireParent(ctx, "AttachBoneToBone");
		SkeletonComponent* parentSkeleton = RequireSkeleton(parent, "AttachBoneToBone");
		RequireSkeleton(&ctx.self, "AttachBoneToBone");

		GameObject* socket = CreateBoneSocket(ctx.objectManager, parentSkeleton, p.parentBone);
		Handle<TransformComponent> attachPoint(socket->GetComponent<BoneSocketComponent>());

		auto* attach = ctx.Add<AttachBoneToBoneComponent>(attachPoint, p.childBone);
		attach->SetLocalRotation(Math::Quaternion::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(p.rotationEulerDeg.x),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.y),
			DirectX::XMConvertToRadians(p.rotationEulerDeg.z)));
		attach->SetLocalPosition(p.position);
		// 持ち主(親)のscaleに追従させる
		attach->SetScaleSource(Handle<TransformComponent>(parent->GetComponent<TransformComponent>()));
	}

	// 親がWeaponSetを持つ場合は、自分を武器としてparams.slotのスロットに登録する。
	void BuildWeapon(BuildContext& ctx, const WeaponParams& p)
	{
		auto* weapon = ctx.Add<WeaponComponent>();
		if (!ctx.parent) return;

		if (auto* weaponSet = ctx.parent->GetComponent<WeaponSetComponent>()) {
			weaponSet->RegisterWeapon(p.slot, Handle<WeaponComponent>(weapon));
		}
	}
}  // namespace

void RegisterCombatComponents(ComponentRegistry& registry)
{
	// --- 戦闘(共通) ---
	registry.Add<HitReactionComponent>("HitReaction");
	registry.Add<WeaponSetComponent>("WeaponSet");
	registry.Add<PostureComponent>("Posture");
	registry.Add<HealthComponent>("Health");
	registry.Add<LifeComponent>("Life");

	// --- 武器・ソケット ---
	registry.AddWithParams<BoneSocketsParams>("BoneSockets", BuildBoneSockets);
	registry.AddWithParams<AttachToBoneParams>("AttachToBone", BuildAttachToBone);
	registry.AddWithParams<AttachBoneToBoneParams>("AttachBoneToBone", BuildAttachBoneToBone);
	registry.AddWithParams<WeaponParams>("Weapon", BuildWeapon);

	registry.AddRaw("AttackSource", [](BuildContext& ctx, const nlohmann::json&) {
		GameObject* owner = RequireParent(ctx, "AttackSource");
		ctx.Add<AttackSourceComponent>()->SetOwner(Handle<GameObject>(owner));
		});

	registry.AddWithParams<TwoBoneIKParams>("TwoBoneIK", [](BuildContext& ctx, const TwoBoneIKParams& p) {
		ctx.Add<TwoBoneIKComponent>(p.rootBone, p.midBone, p.tipParentBone, p.tipBone);
		});
}