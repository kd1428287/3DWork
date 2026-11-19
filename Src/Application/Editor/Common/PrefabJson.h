#pragma once

#include <string>
#include "nlohmann/json.hpp"

// Prefab(ordered_json)を読むエディタ(Action/Collider)共通の読み取りヘルパー
namespace PrefabJson
{
	// GameObject定義から指定typeのコンポーネントを探す(無ければnullptr)
	nlohmann::ordered_json* FindComponent(nlohmann::ordered_json& node, const char* type);

	// ordered_json → json(nlohmann::json専用のfrom_jsonを使うための変換)
	nlohmann::json ToJson(const nlohmann::ordered_json& j);

	// ノードのscale(無い・不正なら1,1,1)
	Math::Vector3 ReadScale(const nlohmann::ordered_json& node);

	// ノードのModelRenderコンポーネントのmodelパス(無ければ空)
	std::string ReadModelPath(nlohmann::ordered_json& node);

	// AttachToBoneコンポーネントのparams
	struct AttachToBone
	{
		std::string			bone;
		Math::Vector3		position = Math::Vector3::Zero;
		Math::Quaternion	rotation = Math::Quaternion::Identity;	// rotationEulerDeg(度)から変換
	};
	AttachToBone ReadAttachToBone(const nlohmann::ordered_json& attachComponent);

	// AttachToBoneの子のワールド行列(実行時と同じ計算)。boneWorldはボーンのワールド行列。
	//	位置=ボーン位置+ボーン回転で回した(position*ownerScale)、回転=rotation*ボーン回転、
	//	スケール=selfScale*ownerScale(ボーンのscaleは使わない)。Decompose失敗時はfalse
	bool ComputeBoneAttachedWorld(const AttachToBone& attach, const Math::Vector3& selfScale,
		const Math::Vector3& ownerScale, const Math::Matrix& boneWorld, Math::Matrix& out);
}
