// 見た目(モデル・アニメーション・描画Mod・エフェクト)とカメラまわりのコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"

#include "Application/Components/Core/BoneSocketComponent.h"

#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/OutlineRenderComponent.h"
#include "Application/Components/Graphics/Render/GroundRenderComponent.h"
#include "Application/Components/Graphics/Render/Mod/RasterizerStateComponent.h"
#include "Application/Components/Graphics/Effect/SlashTrailComponent.h"
#include "Application/Components/Graphics/Effect/FootDustComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Animation/RootMotionApplierComponent.h"
#include "Application/Components/Graphics/Animation/FacingDirectionComponent.h"

#include "Application/Components/Physics/Movement/FollowCameraComponent.h"
#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"
#include "Application/Components/GamePlay/Character/Common/FootstepEventComponent.h"

using namespace Registration;

// ConfigのJSON変換。キー名はメンバ名と同じで、未指定のキーは初期値になる(マクロの説明はComponentParamsOrderedJson.h)。

// 未知の軸名は先頭のYになる。
NLOHMANN_JSON_SERIALIZE_ENUM(RootMotionAxis, {
	{ RootMotionAxis::Y, "Y" },
	{ RootMotionAxis::X, "X" },
	{ RootMotionAxis::Z, "Z" },
	})

// FootSide: JSONでは"Left"/"Right"の文字列で指定する。未知の値はLeftになる。
NLOHMANN_JSON_SERIALIZE_ENUM(FootSide, {
	{ FootSide::Left, "Left" },
	{ FootSide::Right, "Right" },
	})

// KdRasterizerState: JSONではenum名の文字列で指定する。未知の値はCullNoneになる。
NLOHMANN_JSON_SERIALIZE_ENUM(KdRasterizerState, {
	{ KdRasterizerState::CullNone, "CullNone" },
	{ KdRasterizerState::CullFront, "CullFront" },
	{ KdRasterizerState::CullBack, "CullBack" },
	{ KdRasterizerState::WireFrame, "WireFrame" },
	})

COMPONENT_PARAMS_DEFINE_TYPE(SkeletonConfig, model, animations)
COMPONENT_PARAMS_DEFINE_TYPE(RootMotionConfig,
	boneName, unitScale, forwardAxis, forwardSign, rightAxis, rightSign, extractRotation, yawSign)
COMPONENT_PARAMS_DEFINE_TYPE(ModelAnimatorConfig, fps, speedScale, blendDuration, rootMotion)
COMPONENT_PARAMS_DEFINE_TYPE(FollowCameraConfig, offset, followPosition, followRotation)
COMPONENT_PARAMS_DEFINE_TYPE(CameraTargetConfig, offset)
COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailConfig, trailName, base, tip, key, emitOnStart)
COMPONENT_PARAMS_DEFINE_TYPE(OutlineConfig, width)
COMPONENT_PARAMS_DEFINE_TYPE(FacingDirectionConfig, rotationSpeed, moveThreshold)
COMPONENT_PARAMS_DEFINE_TYPE(FootstepTrigger, phase, foot)

// 接続を伴う型のparams。ODR違反を避けるため、マクロも含めて無名名前空間に入れる。
namespace
{
	// 描画中だけラスタライザステートを切り替えるMod。
	struct RasterizerStateParams
	{
		KdRasterizerState state = KdRasterizerState::CullNone;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(RasterizerStateParams, state)

	// 1クリップ分の接地phase定義。clipはアニメーションクリップ名
	// (ModelAnimatorComponentが再生する名前と一致させる)。
	struct FootstepClipParams
	{
		std::string clip;
		std::vector<FootstepTrigger> triggers;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(FootstepClipParams, clip, triggers)

	// 足音ダスト用のパラメータ。左右の足ボーンにBoneSocketComponentを生成し、
	// FootstepEventComponent(接地検知)とFootDustComponent(エフェクト発行)を組み立てる。
	struct FootDustParams
	{
		std::string leftFootBone = "mixamorig:LeftFoot";
		std::string rightFootBone = "mixamorig:RightFoot";
		std::string effectId = "FootDust";
		std::vector<FootstepClipParams> clips;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(FootDustParams, leftFootBone, rightFootBone, effectId, clips)

	// 左右の足ボーンにBoneSocketComponentを生成し、FootstepEventComponent(クリップごとの
	// 接地phase監視)とFootDustComponent(GenericEffectSpawnEvent発行)を組み立てる。
	void BuildFootDust(BuildContext& ctx, const FootDustParams& p)
	{
		SkeletonComponent* skeleton = RequireSkeleton(&ctx.self, "FootDust");

		GameObject* leftSocketObj = CreateBoneSocket(ctx.objectManager, skeleton, p.leftFootBone);
		GameObject* rightSocketObj = CreateBoneSocket(ctx.objectManager, skeleton, p.rightFootBone);

		auto* footstepEvents = ctx.Add<FootstepEventComponent>();
		for (const FootstepClipParams& clip : p.clips) {
			footstepEvents->RegisterClip(clip.clip, clip.triggers);
		}

		auto* footDust = ctx.Add<FootDustComponent>();
		auto left = Handle<BoneSocketComponent>(leftSocketObj->GetComponent<BoneSocketComponent>());
		auto right = Handle<BoneSocketComponent>(rightSocketObj->GetComponent<BoneSocketComponent>());
		footDust->SetFootSockets(left, right);
		footDust->SetEffectId(p.effectId);
	}
}  // namespace

void RegisterGraphicsComponents(ComponentRegistry& registry)
{
	registry.AddWithParams<SkeletonConfig>("ModelRender", [](BuildContext& ctx, const SkeletonConfig& config) {
		auto* skeleton = ctx.Add<SkeletonComponent>();
		skeleton->SetConfig(config);
		ctx.Add<ModelRenderComponent>();
		});
	registry.AddWithParams<ModelAnimatorConfig>("ModelAnimator", [](BuildContext& ctx, const ModelAnimatorConfig& config) {
		// ルートモーションの移動量をモデルの拡大率(水平)に合わせる。
		ModelAnimatorConfig scaled = config;
		scaled.rootMotion.unitScale *= HorizontalScale(ctx);
		ctx.Add<ModelAnimatorComponent>()->SetConfig(scaled);
		});
	registry.Add<RootMotionApplierComponent>("RootMotionApplier");
	registry.Add<FacingDirectionComponent>("FacingDirection");
	registry.Add<FollowCameraComponent>("FollowCamera");
	registry.AddWithParams<CameraTargetConfig>("CameraTarget", [](BuildContext& ctx, const CameraTargetConfig& config) {
		// 注視点のオフセットはscale=1基準。モデルの拡大率に合わせる。
		CameraTargetConfig scaled = config;
		scaled.offset *= ctx.OwnerScale();
		ctx.Add<CameraTargetComponent>()->SetConfig(scaled);
		});
	registry.Add<SlashTrailComponent>("SlashTrail");
	registry.Add<OutlineRenderComponent>("Outline");
	registry.AddWithParams<RasterizerStateParams>("RasterizerState", [](BuildContext& ctx, const RasterizerStateParams& p) {
		ctx.Add<RasterizerStateComponent>(p.state);
		});
	registry.AddWithParams<FootDustParams>("FootDust", BuildFootDust);
	registry.AddRaw("WireFrame", [](BuildContext& ctx, const nlohmann::json&) { AddWireFrameOnce(ctx.self); });
	registry.Add<GroundRenderComponent>("GroundRender");
}
