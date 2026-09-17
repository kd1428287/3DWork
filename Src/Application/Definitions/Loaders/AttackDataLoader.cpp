#include "AttackDataLoader.h"

namespace
{
	using json = nlohmann::json;

	// PlayerDefinitionLoader.cpp/PlayerAttackTableLoader.cpp内の同名ヘルパーと
	// 同じもの。stepDirection(Math::Vector3)を読むためだけに必要な小さな
	// 関数のため、共有ヘッダを新設するほどの規模でもなく、ここでも
	// 自己完結させている(3箇所以上で全く同じものが必要になったら
	// 共有JSONユーティリティへ切り出すこと)。
	Math::Vector3 ReadVector3(const json& j, const Math::Vector3& fallback = {})
	{
		if (!j.is_array() || j.size() != 3) return fallback;
		return Math::Vector3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
	}

	void ReadAnimationSegment(const json& j, AnimationSegment& out)
	{
		out.startFrame = j.value("startFrame", out.startFrame);
		out.endFrame = j.value("endFrame", out.endFrame);
		out.targetDuration = j.value("targetDuration", out.targetDuration);
	}

	void ReadAttackDamageData(const json& j, AttackDamageData& out)
	{
		out.damage = j.value("damage", out.damage);
		out.knockbackPower = j.value("knockbackPower", out.knockbackPower);
		out.hitStunSeconds = j.value("hitStunSeconds", out.hitStunSeconds);
		out.postureDamage = j.value("postureDamage", out.postureDamage);
		out.chipDamageRatio = j.value("chipDamageRatio", out.chipDamageRatio);
		out.parryPostureDamage = j.value("parryPostureDamage", out.parryPostureDamage);
	}

	void ReadAttackMoveData(const json& j, AttackMoveData& out)
	{
		out.stepDistance = j.value("stepDistance", out.stepDistance);
		out.stepDuration = j.value("stepDuration", out.stepDuration);
		out.engageDistance = j.value("engageDistance", out.engageDistance);
		if (j.contains("stepDirection")) out.stepDirection = ReadVector3(j["stepDirection"], out.stepDirection);
		out.blendDuration = j.value("blendDuration", out.blendDuration);
		out.useRootMotion = j.value("useRootMotion", out.useRootMotion);
	}

	void ReadAttackPhaseData(const json& j, AttackPhaseData& out)
	{
		out.animationName = j.value("animationName", out.animationName);
		if (j.contains("windup")) ReadAnimationSegment(j["windup"], out.windup);
		if (j.contains("active")) ReadAnimationSegment(j["active"], out.active);
		if (j.contains("recovery")) ReadAnimationSegment(j["recovery"], out.recovery);
	}

	void ReadAttackCancelData(const json& j, AttackCancelData& out)
	{
		out.recoveryMoveCancelStart = j.value("recoveryMoveCancelStart", out.recoveryMoveCancelStart);
		out.recoveryEvadeCancelStart = j.value("recoveryEvadeCancelStart", out.recoveryEvadeCancelStart);
		out.recoveryAttackCancelStart = j.value("recoveryAttackCancelStart", out.recoveryAttackCancelStart);
		out.comboWindowAfterRecovery = j.value("comboWindowAfterRecovery", out.comboWindowAfterRecovery);
	}
}

void AttackDataLoader::ReadAttackData(const nlohmann::json& j, AttackData& out)
{
	if (j.contains("damageData")) ReadAttackDamageData(j["damageData"], out.damageData);
	if (j.contains("moveData")) ReadAttackMoveData(j["moveData"], out.moveData);
	if (j.contains("phaseData")) ReadAttackPhaseData(j["phaseData"], out.phaseData);
	if (j.contains("cancelData")) ReadAttackCancelData(j["cancelData"], out.cancelData);

	if (j.contains("weaponSlots")) {
		out.weaponSlots.clear();
		for (const auto& slot : j["weaponSlots"]) {
			out.weaponSlots.push_back(slot.get<std::string>());
		}
	}
}
