#pragma once

#include "nlohmann/json.hpp"
#include "Application/Definitions/Loaders/ComponentParamsOrderedJson.h"
#include "../Character/Common/CharacterDefinitionCommon.h"
#include "../Character/Common/CombatData.h"

// 複数キャラで共有するstructのJSON変換。フィールド追加時はstructと下の列挙の両方に足す(要nlohmann 3.11以上)。
// 未指定キーはstructの既定値になる。Player専用の型は各Loaderの先頭に置く。

// Vector3は[x, y, z]の配列で表す。要素数が3でなければ例外にする。
//
// to_json/from_jsonはBasicJsonTypeでテンプレート化してある。ここを具体的な"json"型に
// 固定してしまうと、ComponentRegistryのdefaultParams用に使っているnlohmann::ordered_json
// (Math::Vector3を含むConfig/Params経由で辿り着く)からは呼び出せずコンパイルエラーになる
// (詳細はComponentParamsOrderedJson.hのコメントを参照)
namespace nlohmann
{
	template<>
	struct adl_serializer<Math::Vector3>
	{
		template <typename BasicJsonType>
		static void from_json(const BasicJsonType& j, Math::Vector3& v)
		{
			if (!j.is_array() || j.size() != 3) throw std::runtime_error("Vector3 must be [x, y, z]");
			v = Math::Vector3(j[0].template get<float>(), j[1].template get<float>(), j[2].template get<float>());
		}

		template <typename BasicJsonType>
		static void to_json(BasicJsonType& j, const Math::Vector3& v) { j = BasicJsonType::array({ v.x, v.y, v.z }); }
	};

	// Quaternionは[x, y, z, w]の配列で表す。要素数が4でなければ例外にする。
	template<>
	struct adl_serializer<Math::Quaternion>
	{
		template <typename BasicJsonType>
		static void from_json(const BasicJsonType& j, Math::Quaternion& q)
		{
			if (!j.is_array() || j.size() != 4) throw std::runtime_error("Quaternion must be [x, y, z, w]");
			q = Math::Quaternion(j[0].template get<float>(), j[1].template get<float>(), j[2].template get<float>(), j[3].template get<float>());
		}

		template <typename BasicJsonType>
		static void to_json(BasicJsonType& j, const Math::Quaternion& q) { j = BasicJsonType::array({ q.x, q.y, q.z, q.w }); }
	};
}

COMPONENT_PARAMS_DEFINE_TYPE(MotionClipData,
	animationName, loop, duration, useRootMotion, blendDuration, startFrame, endFrame)

	COMPONENT_PARAMS_DEFINE_TYPE(AttackDamageData,
		damage, knockbackPower, hitStunSeconds, postureDamage, chipDamageRatio, parryPostureDamage)

	COMPONENT_PARAMS_DEFINE_TYPE(AttackPhaseData, windup, active, recovery)

	COMPONENT_PARAMS_DEFINE_TYPE(AttackMoveData,
		stepDistance, stepDuration, engageDistance, stepDirection)

	COMPONENT_PARAMS_DEFINE_TYPE(AttackCancelData,
		recoveryMoveCancelStart, recoveryEvadeCancelStart, recoveryAttackCancelStart, comboWindowAfterRecovery)

	COMPONENT_PARAMS_DEFINE_TYPE(AttackData,
		damageData, moveData, phaseData, cancelData, weaponSlots)