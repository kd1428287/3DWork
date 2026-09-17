#include "PlayerDefinitionLoader.h"

#include <unordered_map>
#include <nlohmann/json.hpp>

#include "JsonLoader.h"
#include "Application/Definitions/Character/Player/PlayerCombatDataTable.h"
#include "PlayerAttackTableLoader.h"

namespace
{
	using json = nlohmann::json;

	Math::Vector3 ReadVector3(const json& j, const Math::Vector3& fallback = {})
	{
		if (!j.is_array() || j.size() != 3) return fallback;
		return Math::Vector3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
	}

	ColliderCategory ToColliderCategory(const std::string& name)
	{
		static const std::unordered_map<std::string, ColliderCategory> table = {
			{ "None",    ColliderCategory::None },
			{ "Bump",    ColliderCategory::Bump },
			{ "HitBox",  ColliderCategory::HitBox },
			{ "HurtBox", ColliderCategory::HurtBox },
		};
		auto it = table.find(name);
		// 未知のカテゴリ名は「衝突なし」に倒す(ここは要件次第でassertに変えてもよい)。
		return it != table.end() ? it->second : ColliderCategory::None;
	}

	RootMotionAxis ToRootMotionAxis(const std::string& name)
	{
		if (name == "X") return RootMotionAxis::X;
		if (name == "Z") return RootMotionAxis::Z;
		return RootMotionAxis::Y;
	}

	void ReadIKChain(const json& j, IKChainDefinition& out)
	{
		out.rootBone = j.value("rootBone", out.rootBone);
		out.midBone = j.value("midBone", out.midBone);
		out.tipParentBone = j.value("tipParentBone", out.tipParentBone);
		out.tipBone = j.value("tipBone", out.tipBone);
	}

	void ReadColliders(const json& arr, std::vector<CapsuleColliderDefinition>& out)
	{
		for (const auto& c : arr) {
			CapsuleColliderDefinition def;
			def.name = c.value("name", std::string());
			def.radius = c.value("radius", 0.0f);
			def.start = ReadVector3(c["start"]);
			def.end = ReadVector3(c["end"]);
			def.category = ToColliderCategory(c.value("category", std::string("Bump")));
			def.interactsWith = ToColliderCategory(c.value("interactsWith", std::string("None")));
			def.isTrigger = c.value("isTrigger", false);
			out.push_back(def);
		}
	}

	// ------------------------------------------------------------
	// 戦闘の振る舞い(コンボ木/回避/ガード)。PlayerCombatBehaviorDefinition
	// (PlayerCombatBehaviorDefinition.h)を組み立てる。
	//
	// 【attackTableについて】
	// 攻撃1技あたりのデータ量・技数が今後増えていく前提のため、
	// PlayerAttackTable本体の読み込みはPlayerAttackTableLoaderへ分離した
	// (PlayerAttackTableLoader.h参照)。ここではcombatBehavior.
	// attackTablePathに書かれたパスをそちらへ委譲するだけにする。
	// ------------------------------------------------------------

	void ReadEvadeData(const json& j, EvadeData& out)
	{
		out.activeDuration = j.value("activeDuration", out.activeDuration);
		out.recoveryDuration = j.value("recoveryDuration", out.recoveryDuration);
		out.justWindowStart = j.value("justWindowStart", out.justWindowStart);
		out.justWindowEnd = j.value("justWindowEnd", out.justWindowEnd);
		out.evadeDistance = j.value("evadeDistance", out.evadeDistance);
		out.useRootMotion = j.value("useRootMotion", out.useRootMotion);

		out.animationNameForward = j.value("animationNameForward", out.animationNameForward);
		out.animationNameBackward = j.value("animationNameBackward", out.animationNameBackward);
		out.animationNameLeft = j.value("animationNameLeft", out.animationNameLeft);
		out.animationNameRight = j.value("animationNameRight", out.animationNameRight);
		// evadeDirectionは入力方向から毎回計算し直される値(PlayerStatusController::
		// TryStartEvade参照)であり、データとして持たせる意味が無いため読まない。
	}

	void ReadGuardData(const json& j, GuardData& out)
	{
		out.justWindowDuration = j.value("justWindowDuration", out.justWindowDuration);
		out.animationName = j.value("animationName", out.animationName);
		out.startDuration = j.value("startDuration", out.startDuration);
		out.loopAnimationName = j.value("loopAnimationName", out.loopAnimationName);
		out.parrySuccessAnimationName = j.value("parrySuccessAnimationName", out.parrySuccessAnimationName);
		out.parrySuccessDuration = j.value("parrySuccessDuration", out.parrySuccessDuration);
		out.guardHitAnimationName = j.value("guardHitAnimationName", out.guardHitAnimationName);
		out.guardHitDuration = j.value("guardHitDuration", out.guardHitDuration);
	}

	void ReadCombatBehavior(const json& j, PlayerCombatBehaviorDefinition& out)
	{
		// attackTablePathが指定されていれば専用ファイルから読む。読み込みに
		// 失敗した場合(パス間違い・ファイル不備)は、outに既に入っている値
		// (呼び出し元がCreateDebugPlayerCombatBehavior()で埋めておいたもの)
		// をそのまま残す(コライダー等と違い、コンボ木が空になって
		// 攻撃不能になるより、デバッグ値で動く方が実害が少ないため)。
		if (j.contains("attackTablePath")) {
			PlayerAttackTableLoader::LoadFromFile(j["attackTablePath"].get<std::string>(), out.attackTable);
		}
		if (j.contains("evade")) ReadEvadeData(j["evade"], out.evade);
		if (j.contains("guard")) ReadGuardData(j["guard"], out.guard);
	}

	// ------------------------------------------------------------
	// 移動(Walk/Run/ターン)のアニメーション定義。PlayerMovementAnimationDefinition
	// (PlayerCombatBehaviorDefinition.h)を組み立てる。
	// ------------------------------------------------------------

	void ReadMotionClipData(const json& j, MotionClipData& out)
	{
		out.duration = j.value("duration", out.duration);
		out.animationName = j.value("animationName", out.animationName);
		out.useRootMotion = j.value("useRootMotion", out.useRootMotion);
		out.blendDuration = j.value("blendDuration", out.blendDuration);
	}

	void ReadMovementPhaseClips(const json& j, MovementPhaseClips& out)
	{
		out.startAnimationName = j.value("startAnimationName", out.startAnimationName);
		out.startDuration = j.value("startDuration", out.startDuration);
		out.loopAnimationName = j.value("loopAnimationName", out.loopAnimationName);
		out.endAnimationName = j.value("endAnimationName", out.endAnimationName);
		out.endDuration = j.value("endDuration", out.endDuration);
	}

	void ReadWalkLockedAnimationSet(const json& j, WalkLockedAnimationSet& out)
	{
		out.forward = j.value("forward", out.forward);
		out.forwardRight = j.value("forwardRight", out.forwardRight);
		out.right = j.value("right", out.right);
		out.backwardRight = j.value("backwardRight", out.backwardRight);
		out.backward = j.value("backward", out.backward);
		out.backwardLeft = j.value("backwardLeft", out.backwardLeft);
		out.left = j.value("left", out.left);
		out.forwardLeft = j.value("forwardLeft", out.forwardLeft);
	}

	void ReadWalkAnimationSet(const json& j, WalkAnimationSet& out)
	{
		if (j.contains("forward")) ReadMovementPhaseClips(j["forward"], out.forward);
		if (j.contains("locked")) ReadWalkLockedAnimationSet(j["locked"], out.locked);
	}

	void ReadTurnAnimationSet(const json& j, TurnAnimationSet& out)
	{
		if (j.contains("left90")) ReadMotionClipData(j["left90"], out.left90);
		if (j.contains("right90")) ReadMotionClipData(j["right90"], out.right90);
		if (j.contains("left180")) ReadMotionClipData(j["left180"], out.left180);
		if (j.contains("right180")) ReadMotionClipData(j["right180"], out.right180);
	}

	void ReadMovementAnimations(const json& j, PlayerMovementAnimationDefinition& out)
	{
		if (j.contains("walk")) ReadWalkAnimationSet(j["walk"], out.walk);
		if (j.contains("run")) ReadMovementPhaseClips(j["run"], out.run);
		if (j.contains("turn")) ReadTurnAnimationSet(j["turn"], out.turn);
	}
}

bool PlayerDefinitionLoader::LoadFromFile(const std::string& path, PlayerDefinition& outDefinition)
{
	json root;
	if (!JsonLoader::Load(path, root)) return false;

	PlayerDefinition def;

	// --- 見た目 ---
	const json& visuals = root["visuals"];
	def.visuals.modelPath = visuals.value("modelPath", std::string());
	def.visuals.animatorFPS = visuals.value("animatorFPS", 60);
	def.visuals.rootMotionBoneName = visuals.value("rootMotionBoneName", std::string());
	def.visuals.rootMotionAxis = ToRootMotionAxis(visuals.value("rootMotionAxis", std::string("Y")));
	def.visuals.rootMotionAxisSign = visuals.value("rootMotionAxisSign", -1.0f);
	def.visuals.rootMotionScale = visuals.value("rootMotionScale", 0.01f);

	// --- 戦闘数値 ---
	def.combatStats.maxHealth = root["combatStats"].value("maxHealth", 100.0f);

	// --- 戦闘の振る舞い(コンボ木/回避/ガード) ---
	// まずPlayerCombatDataTable::CreateDebugPlayerCombatBehavior()の
	// デバッグ値で埋めておき、ファイル側にcombatBehaviorセクションが
	// あればその内容で上書きする。ファイルが一部のキー(例:guardだけ)
	// しか指定していなくても、残りはデバッグ値のまま動く
	// (ReadCombatBehavior/ReadEvadeData/ReadGuardData等は「out(既定値)を
	// 上書きする」方式で統一しているため)。
	def.combatBehavior = CreateDebugPlayerCombatBehavior();
	if (root.contains("combatBehavior")) {
		ReadCombatBehavior(root["combatBehavior"], def.combatBehavior);
	}

	// --- 移動アニメーション ---
	// 上と同じ方針で、デバッグ値をベースにファイル側の指定で上書きする。
	def.movementAnimations = CreateDebugPlayerMovementAnimations();
	if (root.contains("movementAnimations")) {
		ReadMovementAnimations(root["movementAnimations"], def.movementAnimations);
	}

	// --- コライダー ---
	if (root.contains("colliders")) ReadColliders(root["colliders"], def.colliders);

	// --- 移動 ---
	def.walkSpeed = root.value("walkSpeed", 2.0f);
	def.runSpeed = root.value("runSpeed", def.runSpeed);

	// --- ソケット ---
	if (root.contains("auxiliarySocketBones")) {
		for (const auto& b : root["auxiliarySocketBones"]) {
			def.auxiliarySocketBones.push_back(b.get<std::string>());
		}
	}
	def.weaponSocketBone = root.value("weaponSocketBone", std::string());

	// --- 右腕IK ---
	if (root.contains("rightArmIK")) ReadIKChain(root["rightArmIK"], def.rightArmIK);

	// --- 武器 ---
	const json& weapon = root["weapon"];
	def.weapon.modelPath = weapon.value("modelPath", std::string());
	def.weapon.socketLocalPosition = ReadVector3(weapon["socketLocalPosition"]);
	def.weapon.socketLocalEulerRotationDeg = ReadVector3(weapon["socketLocalEulerRotationDeg"]);
	def.weapon.hitBox.halfExtents = ReadVector3(weapon["hitBox"]["halfExtents"]);
	def.weapon.hitBox.offset = ReadVector3(weapon["hitBox"]["offset"]);

	outDefinition = std::move(def);
	return true;
}
