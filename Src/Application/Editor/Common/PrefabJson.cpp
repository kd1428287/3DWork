#include "Application/main.h"

#include "PrefabJson.h"
#include "Application/Definitions/Loaders/DefinitionJson.h"	

namespace PrefabJson
{
	nlohmann::ordered_json* FindComponent(nlohmann::ordered_json& node, const char* type)
	{
		if (!node.contains("components") || !node["components"].is_array()) return nullptr;

		for (auto& c : node["components"])
		{
			if (c.value("type", std::string()) == type) return &c;
		}
		return nullptr;
	}

	nlohmann::json ToJson(const nlohmann::ordered_json& j)
	{
		return nlohmann::json::parse(j.dump());
	}

	Math::Vector3 ReadScale(const nlohmann::ordered_json& node)
	{
		try
		{
			if (node.contains("scale")) { return ToJson(node["scale"]).get<Math::Vector3>(); }
		}
		catch (const nlohmann::json::exception&) {}
		return Math::Vector3(1, 1, 1);
	}

	std::string ReadModelPath(nlohmann::ordered_json& node)
	{
		if (nlohmann::ordered_json* mr = FindComponent(node, "ModelRender"))
		{
			if (mr->contains("params")) { return (*mr)["params"].value("model", std::string()); }
		}
		return std::string();
	}

	AttachToBone ReadAttachToBone(const nlohmann::ordered_json& attachComponent)
	{
		AttachToBone a;

		const nlohmann::json params = attachComponent.contains("params") ? ToJson(attachComponent["params"]) : nlohmann::json::object();
		a.bone = params.value("bone", std::string());

		try
		{
			if (params.contains("position")) { a.position = params["position"].get<Math::Vector3>(); }
			if (params.contains("rotationEulerDeg") && params["rotationEulerDeg"].is_array() && params["rotationEulerDeg"].size() >= 3)
			{
				const auto& e = params["rotationEulerDeg"];
				a.rotation = Math::Quaternion::CreateFromYawPitchRoll(
					DirectX::XMConvertToRadians(e[1].get<float>()),
					DirectX::XMConvertToRadians(e[0].get<float>()),
					DirectX::XMConvertToRadians(e[2].get<float>()));
			}
		}
		catch (const nlohmann::json::exception&) {}

		return a;
	}

	bool ComputeBoneAttachedWorld(const AttachToBone& attach, const Math::Vector3& selfScale,
		const Math::Vector3& ownerScale, const Math::Matrix& boneWorld, Math::Matrix& out)
	{
		Math::Matrix m = boneWorld;
		Math::Vector3 boneScale, bonePos;
		Math::Quaternion boneRot;
		if (!m.Decompose(boneScale, boneRot, bonePos)) return false;

		const Math::Vector3 rotatedOffset = Math::Vector3::Transform(attach.position * ownerScale, boneRot);

		out = Math::Matrix::CreateScale(selfScale * ownerScale)
			* Math::Matrix::CreateFromQuaternion(attach.rotation * boneRot)
			* Math::Matrix::CreateTranslation(bonePos + rotatedOffset);
		return true;
	}
}