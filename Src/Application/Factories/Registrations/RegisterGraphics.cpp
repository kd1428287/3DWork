// 見た目(モデル・アニメーション・描画Mod・エフェクト)とカメラまわりのコンポーネント登録。

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Factories/Registrations/RegisterFunctions.h"
#include "Application/Factories/Registrations/RegistrationHelpers.h"
#include "Application/Definitions/Json/KdJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"
#include "Application/Factories/Json/GraphicsConfigJson.h"
#include "Application/Definitions/Prefab/BitFlags.h"

#include "Application/Components/Core/BoneSocketComponent.h"

#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/OutlineRenderComponent.h"
#include "Application/Components/Graphics/Render/AuraRenderComponent.h"
#include "Application/Components/Graphics/Render/GroundRenderComponent.h"
#include "Application/Components/Graphics/Render/Mod/RasterizerStateComponent.h"
#include "Application/Components/Graphics/Render/Mod/InkStyleComponent.h"
#include "Application/Components/Graphics/Effect/SlashTrailComponent.h"
#include "Application/Components/Graphics/Effect/FootDustComponent.h"
#include "Application/Components/Graphics/Effect/WindShellComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Animation/RootMotionApplierComponent.h"
#include "Application/Components/Graphics/Animation/FacingDirectionComponent.h"
#include "Application/Components/Graphics/Effect/SlashTrailComponent.h"

#include "Application/Components/Physics/Movement/FollowCameraComponent.h"
#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"
#include "Application/Components/GamePlay/Character/Common/FootstepEventComponent.h"

using namespace Registration;

// 接続を伴う型のparams。ODR違反を避けるため、マクロも含めて無名名前空間に入れる。
namespace
{
	// 描画中だけラスタライザステートを切り替えるMod。
	struct RasterizerStateParams
	{
		KdRasterizerState state = KdRasterizerState::CullNone;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(RasterizerStateParams, state)

		// 墨の濃淡変換のMod。JSONではプリセット名("Bright"/"Dark")を指定する。
		struct InkStyleParams
	{
		KdInkPreset preset = KdInkPreset::None;
	};
	COMPONENT_PARAMS_DEFINE_TYPE(InkStyleParams, preset)

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

		struct ModelRenderConfig
	{
		SkeletonConfig skeleton;
		BitFlags<RenderLayer> layer = []() {
			BitFlags<RenderLayer> f;
			f.add(RenderLayer::DrawLit);
			f.add(RenderLayer::GenerateDepthMapFromLight);
			//f.add(RenderLayer::DrawCharaShadow);
			return f;
			}();
	};
	COMPONENT_PARAMS_DEFINE_TYPE(ModelRenderConfig, skeleton, layer)

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
	registry.AddWithParams<ModelRenderConfig>("ModelRender", [](BuildContext& ctx, const ModelRenderConfig& config) {
		auto* skeleton = ctx.Add<SkeletonComponent>();
		skeleton->SetConfig(config.skeleton);
		ctx.Add<ModelRenderComponent>()->SetLayer(static_cast<RenderLayer>(config.layer.raw()));
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
	registry.AddWithParams<SlashTrailConfig>("SlashTrail", [](BuildContext& ctx, const SlashTrailConfig& p) {
		SlashTrailConfig params = p;
		SlashTrailShape shape;
		shape.base = { 0,0,0 };
		shape.tip = { 1,1,1 };
		params.shapes.insert({ "default", shape });
		ctx.Add<SlashTrailComponent>()->SetConfig(params);
		});
	registry.Add<OutlineRenderComponent>("Outline");
	registry.Add<AuraRenderComponent>("Aura");
	registry.AddWithParams<RasterizerStateParams>("RasterizerState", [](BuildContext& ctx, const RasterizerStateParams& p) {
		ctx.Add<RasterizerStateComponent>(p.state);
		});
	registry.AddWithParams<InkStyleParams>("InkStyle", [](BuildContext& ctx, const InkStyleParams& p) {
		ctx.Add<InkStyleComponent>(KdInkStyle::FromPreset(p.preset));
		});
	registry.AddWithParams<FootDustParams>("FootDust", BuildFootDust);
	registry.AddRaw("WireFrame", [](BuildContext& ctx, const nlohmann::json&) { AddWireFrameOnce(ctx.self); });
	registry.Add<GroundRenderComponent>("GroundRender");
	registry.AddWithParams<WindShellConfig>("WindShell", [](BuildContext& ctx, const WindShellConfig& config) {
		ctx.Add<WindShellComponent>()->SetConfig(config);
		});
	registry.AddWithParams<FluidFloorConfig>("FluidFloor", [](BuildContext& ctx, const FluidFloorConfig& config) {
		ctx.Add<FluidFloorComponent>(config);
		});
}