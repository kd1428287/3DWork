#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"
#include "../Character/Common/CharacterDefinitionCommon.h"
#include "../Character/Common/CombatData.h"

// 複数キャラで共有するstructのJSON変換。フィールド追加時はstructと下の列挙の両方に足す(要nlohmann 3.11以上)。
// 未指定キーはstructの既定値になる。Player専用の型は各Loaderの先頭に置く。

// Vector3は[x, y, z]の配列で表す。要素数が3でなければ例外にする。
namespace nlohmann
{
	template<>
	struct adl_serializer<Math::Vector3>
	{
		static void from_json(const json& j, Math::Vector3& v)
		{
			if (!j.is_array() || j.size() != 3) throw std::runtime_error("Vector3 must be [x, y, z]");
			v = Math::Vector3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
		}

		static void to_json(json& j, const Math::Vector3& v) { j = json::array({ v.x, v.y, v.z }); }
	};

	// Quaternionは[x, y, z, w]の配列で表す。要素数が4でなければ例外にする。
	template<>
	struct adl_serializer<Math::Quaternion>
	{
		static void from_json(const json& j, Math::Quaternion& q)
		{
			if (!j.is_array() || j.size() != 4) throw std::runtime_error("Quaternion must be [x, y, z, w]");
			q = Math::Quaternion(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
		}

		static void to_json(json& j, const Math::Quaternion& q) { j = json::array({ q.x, q.y, q.z, q.w }); }
	};
}

// HitReactionFlagsは名前の配列で表す(例: ["CameraShake", "HitStop"])。未知の名前は例外にする。
namespace DefinitionJsonDetail
{
	inline const std::pair<const char*, HitReactionFlags> kHitReactionFlagNames[] = {
		{ "CameraShake",   HitReactionFlags::CameraShake },
		{ "HitStop",       HitReactionFlags::HitStop },
		{ "WeaponClashFx", HitReactionFlags::WeaponClashFx },
	};
}

template<typename BasicJsonType>
inline void from_json(const BasicJsonType& j, HitReactionFlags& flags)
{
	flags = HitReactionFlags::None;
	for (const std::string& name : j.template get<std::vector<std::string>>()) {
		bool found = false;
		for (const auto& entry : DefinitionJsonDetail::kHitReactionFlagNames) {
			if (name == entry.first) {
				flags |= entry.second;
				found = true;
				break;
			}
		}
		if (!found) throw std::runtime_error("Unknown HitReactionFlags: " + name);
	}
}

template<typename BasicJsonType>
inline void to_json(BasicJsonType& j, const HitReactionFlags& flags)
{
	j = BasicJsonType::array();
	for (const auto& entry : DefinitionJsonDetail::kHitReactionFlagNames) {
		if (HasFlag(flags, entry.second)) j.push_back(entry.first);
	}
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HitReactionConfig,
	effectFlags, damageEffectName, cameraShakeIntensity, hitStopDelaySeconds,
	hitStopDurationSeconds, guardKnockbackPower, largeStaggerDuration)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MotionClipData,
	animationName, duration, useRootMotion, blendDuration)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AttackDamageData,
	damage, knockbackPower, hitStunSeconds, postureDamage, chipDamageRatio, parryPostureDamage)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AnimationSegment, startFrame, endFrame, targetDuration)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AttackPhaseData, animationName, windup, active, recovery)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AttackMoveData,
	stepDistance, stepDuration, engageDistance, stepDirection, blendDuration, useRootMotion)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AttackCancelData,
	recoveryMoveCancelStart, recoveryEvadeCancelStart, recoveryAttackCancelStart, comboWindowAfterRecovery)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AttackData,
	damageData, moveData, phaseData, cancelData, weaponSlots)
