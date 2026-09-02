#include "PlayerDefinitionLoader.h"

#include <fstream>
#include <unordered_map>
#include <nlohmann/json.hpp>

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
}

bool PlayerDefinitionLoader::LoadFromFile(const std::string& path, PlayerDefinition& outDefinition)
{
	std::ifstream file(path);
	if (!file.is_open()) return false;

	json root;
	try {
		file >> root;
	}
	catch (const json::parse_error&) {
		return false;
	}

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

	// --- コライダー ---
	if (root.contains("colliders")) ReadColliders(root["colliders"], def.colliders);

	// --- 移動 ---
	def.walkSpeed = root.value("walkSpeed", 2.0f);

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
