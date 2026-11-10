#pragma once

#include "nlohmann/json.hpp"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "DefinitionJson.h"	// Vector3の[x,y,z]変換を使う為
#include "../Effect/SlashTrailParams.h"

// SlashTrailParamsのJSON変換(DefinitionJson.hと同じ流儀)。フィールド追加時はstructと下の列挙の両方に足す。
// 未指定キーはstructの既定値になる。キー名はメンバ名そのまま。
// json依存をParams.hに持ち込まない為、変換が必要なcpp(EffectDataLoader等)だけがこのヘッダーを読み込む。

// ParticleBlendModeは文字列で表す。未知の文字列は先頭のAddになる(EffectDataLoaderの手書き変換と同じ)。
NLOHMANN_JSON_SERIALIZE_ENUM(ParticleBlendMode, {
	{ ParticleBlendMode::Add, "Add" },
	{ ParticleBlendMode::Alpha, "Alpha" },
	{ ParticleBlendMode::Multiply, "Multiply" },
	{ ParticleBlendMode::LiquidInk, "LiquidInk" },
	})

namespace nlohmann
{
	// ParticleDrawPassはビットフラグなので、立っているものだけを["Default","Bright"]の配列で表す
	// (エフェクトのJSONと同じ形式)。単一文字列は旧形式として読める。読めるものが無ければDefaultになる
	template<>
	struct adl_serializer<ParticleDrawPass>
	{
		template <typename BasicJsonType>
		static void to_json(BasicJsonType& j, const ParticleDrawPass& flags)
		{
			j = BasicJsonType::array();
			if (KdHasDrawPassFlag(flags, ParticleDrawPass::Default)) { j.push_back("Default"); }
			if (KdHasDrawPassFlag(flags, ParticleDrawPass::Bright)) { j.push_back("Bright"); }
		}

		template <typename BasicJsonType>
		static void from_json(const BasicJsonType& j, ParticleDrawPass& flags)
		{
			ParticleDrawPass result = static_cast<ParticleDrawPass>(0);
			bool any = false;

			auto apply = [&](const std::string& s)
				{
					if (s == "Default") { result |= ParticleDrawPass::Default; any = true; }
					else if (s == "Bright") { result |= ParticleDrawPass::Bright; any = true; }
				};

			if (j.is_array())
			{
				for (const auto& entry : j)
				{
					if (entry.is_string()) { apply(entry.template get<std::string>()); }
				}
			}
			else if (j.is_string())
			{
				apply(j.template get<std::string>());
			}

			flags = any ? result : ParticleDrawPass::Default;
		}
	};
}

// サブ構造体を先に登録する(SlashTrailParamsがメンバとして使うため)
COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailRecordParams,
	MinSampleDistance, MaxSamples, MaxRecordingTime, SpeedSmoothingTime)

COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailStrokeParams,
	Length, HeadTaperLength)

COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailWidthParams,
	SpeedReference, MinScale, MaxScale)

COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailSplineParams,
	Enabled, Subdivisions, MaxPoints)

COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailDissolveParams,
	Time, NoiseTexturePath, NoiseScaleU, NoiseScaleV, ScrollU, ScrollV, Softness, EdgeWidth, EdgeColor, EdgeIntensity)

COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailInkParams,
	ColdColor, HotColor, BlendMode, DrawPassFlags)

COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailCoreParams,
	Enabled, WidthScale, Color, TailAlpha, BlendMode, DrawPassFlags)

COMPONENT_PARAMS_DEFINE_TYPE(SlashTrailParams,
	Record, Stroke, Width, Spline, Dissolve, Ink, Core, TexturePath)
