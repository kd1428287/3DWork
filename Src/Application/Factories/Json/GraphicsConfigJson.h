#pragma once

#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Render/OutlineRenderComponent.h"
#include "Application/Components/Graphics/Render/AuraRenderComponent.h"
#include "Application/Components/Graphics/Render/Mod/RasterizerStateComponent.h"
#include "Application/Components/Graphics/Render/Mod/InkStyleComponent.h"
#include "Application/Components/Graphics/Effect/WindShellComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Animation/ModelAnimatorComponent.h"
#include "Application/Components/Graphics/Animation/RootMotionApplierComponent.h"
#include "Application/Components/Graphics/Animation/FacingDirectionComponent.h"
#include "Application/Components/Physics/Movement/FollowCameraComponent.h"
#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"
#include "Application/Components/GamePlay/Character/Common/FootstepEventComponent.h"
#include "Application/Components/Graphics/Effect/SlashTrailComponent.h"

#include "Application/Definitions/Json/KdJson.h"
#include "Application/Definitions/Prefab/BitFlags.h"
#include "Application/Definitions/Character/Common/FootstepTypesJson.h"

// Graphics系ComponentのConfigとenumのJSON変換。キーはメンバ名と同じで、未指定のキーは初期値になる。

// BitFlags<RenderLayer>を名前の配列で読み書きする表。
template <>
struct EnumFlagStringMap<RenderLayer>
{
	static constexpr std::pair<const char*, RenderLayer> entries[] = {
		{ "PreDraw", RenderLayer::PreDraw },
		{ "GenerateDepthMapFromLight", RenderLayer::GenerateDepthMapFromLight },
		{ "DrawUnLit", RenderLayer::DrawUnLit },
		{ "DrawLit", RenderLayer::DrawLit },
		{ "DrawEffect", RenderLayer::DrawEffect },
		{ "DrawBright", RenderLayer::DrawBright },
		{ "DrawSprite", RenderLayer::DrawSprite },
		{ "DrawDebug", RenderLayer::DrawDebug },
		{ "DrawCharaShadow", RenderLayer::DrawCharaShadow },
	};
};

// 未知の軸名は先頭のYになる。
NLOHMANN_JSON_SERIALIZE_ENUM(RootMotionAxis, {
	{ RootMotionAxis::Y, "Y" },
	{ RootMotionAxis::X, "X" },
	{ RootMotionAxis::Z, "Z" },
	})

// KdRasterizerState: JSONではenum名の文字列で指定する。未知の値はCullNoneになる。
NLOHMANN_JSON_SERIALIZE_ENUM(KdRasterizerState, {
	{ KdRasterizerState::CullNone, "CullNone" },
	{ KdRasterizerState::CullFront, "CullFront" },
	{ KdRasterizerState::CullBack, "CullBack" },
	{ KdRasterizerState::WireFrame, "WireFrame" },
	})

// KdInkPreset: JSONではプリセット名の文字列で指定する。未知の値はNoneになる。
NLOHMANN_JSON_SERIALIZE_ENUM(KdInkPreset, {
	{ KdInkPreset::None, "None" },
	{ KdInkPreset::Bright, "Bright" },
	{ KdInkPreset::Dark, "Dark" },
	})

// KdAuraKind: JSONでは"Rim"/"Flame"/"Ink"の文字列で指定する。未知の値はRimになる。
NLOHMANN_JSON_SERIALIZE_ENUM(KdAuraKind, {
	{ KdAuraKind::Rim, "Rim" },
	{ KdAuraKind::Flame, "Flame" },
	{ KdAuraKind::Ink, "Ink" },
	})

// RenderLayer単体を名前の文字列で読み書きする。
NLOHMANN_JSON_SERIALIZE_ENUM(RenderLayer, {
	{ RenderLayer::PreDraw, "PreDraw" },
	{ RenderLayer::GenerateDepthMapFromLight, "GenerateDepthMapFromLight" },
	{ RenderLayer::DrawUnLit, "DrawUnLit" },
	{ RenderLayer::DrawLit, "DrawLit" },
	{ RenderLayer::DrawEffect, "DrawEffect" },
	{ RenderLayer::DrawBright, "DrawBright" },
	{ RenderLayer::DrawSprite, "DrawSprite" },
	{ RenderLayer::DrawDebug, "DrawDebug" },
	{ RenderLayer::DrawCharaShadow, "DrawCharaShadow" },
	})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(KdInkStyle,
	Black, Amount, Paper, Bias, Contrast, Steps, LumaScale, NoiseAmount, NoiseScale)

COMPONENT_PARAMS_DEFINE_TYPE(SkeletonConfig, model, animations)
COMPONENT_PARAMS_DEFINE_TYPE(RootMotionConfig,
	boneName, unitScale, forwardAxis, forwardSign, rightAxis, rightSign, extractRotation, yawSign)
COMPONENT_PARAMS_DEFINE_TYPE(ModelAnimatorConfig, fps, speedScale, blendDuration, rootMotion)
COMPONENT_PARAMS_DEFINE_TYPE(FollowCameraConfig, offset, followPosition, followRotation)
COMPONENT_PARAMS_DEFINE_TYPE(CameraTargetConfig, offset)
COMPONENT_PARAMS_DEFINE_TYPE(OutlineConfig, width)
COMPONENT_PARAMS_DEFINE_TYPE(AuraConfig,
	kind, color, coreColor, intensity, width, rise, height, softness,
	fresnelPower, noiseScale, scrollSpeed, layers, noiseTexturePath)
COMPONENT_PARAMS_DEFINE_TYPE(FacingDirectionConfig, rotationSpeed, moveThreshold)
COMPONENT_PARAMS_DEFINE_TYPE(WindShellConfig,
	preset, localAxis, axisOffset, useVelocity, speedThreshold, axisFollow, autoPlay)
COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailShape, base, tip)
COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailConfig, trailName, emitOnStart, shapes)
