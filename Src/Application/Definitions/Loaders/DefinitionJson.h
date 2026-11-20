#pragma once

#include "nlohmann/json.hpp"
#include "Application/Definitions/Json/KdJson.h"
#include "../Character/Common/CharacterDefinitionCommon.h"
#include "../Character/Common/CombatData.h"

// 複数キャラで共有するstructのJSON変換。フィールド追加時はstructと下の列挙の両方に足す(要nlohmann 3.11以上)。
// 未指定キーはstructの既定値になる。Player専用の型は各Loaderの先頭に置く。
// Vector3/QuaternionはCoreJson.h(KdJson.h経由)で変換済み。

// SegmentEaseはJSON上で文字列("Linear"等)にする。未知の文字列は先頭のLinearになる。
NLOHMANN_JSON_SERIALIZE_ENUM(SegmentEase, {
	{ SegmentEase::Linear, "Linear" },
	{ SegmentEase::EaseIn, "EaseIn" },
	{ SegmentEase::EaseOut, "EaseOut" },
	{ SegmentEase::EaseInOut, "EaseInOut" },
	})

// MotionClipDataがsegmentsとして使うため、先に登録しておく
COMPONENT_PARAMS_DEFINE_TYPE(MotionSegment, from, to, weight, ease)

COMPONENT_PARAMS_DEFINE_TYPE(MotionClipData,
	animationName, loop, duration, useRootMotion, blendDuration, startFrame, endFrame, isForce, segments)

COMPONENT_PARAMS_DEFINE_TYPE(AttackChargeData,
	isEnabled, chargeTime, clip, failedAttackId)


COMPONENT_PARAMS_DEFINE_TYPE(AttackDamageData,
	damage, postureDamage, guardDamageRatio, guardPostureDamageRatio, parryPostureDamage, isParryable, isReboundable)

COMPONENT_PARAMS_DEFINE_TYPE(ReactionEventConfig,
	enableCameraShake, enableHitStop, enableEffect, enableDistortion, effectName, SEName,
	sePitchRandom, useReflectDirection, normalBlendRatio, cameraShakeIntensity, hitStopDurationSeconds,
	recoilChain, recoilStrength)

COMPONENT_PARAMS_DEFINE_TYPE(AttackVisualData, hit, block, parry, hitStopDelaySeconds, trailName, trailShape, windPreset)

COMPONENT_PARAMS_DEFINE_TYPE(AttackPhaseData, windup, active, recovery)

COMPONENT_PARAMS_DEFINE_TYPE(AttackMoveData,
	stepDistance, stepDuration, engageDistance, stepDirection)

COMPONENT_PARAMS_DEFINE_TYPE(AttackCancelData,
	recoveryMoveCancelStart, recoveryEvadeCancelStart, recoveryAttackCancelStart, comboWindowAfterRecovery)

COMPONENT_PARAMS_DEFINE_TYPE(AttackData,
	chargeData, damageData, visualData, moveData, phaseData, cancelData, weaponSlots)
