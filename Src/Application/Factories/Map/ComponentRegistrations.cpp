#include "ComponentTypes.h"

// ここでのみ実際のコンポーネントクラスに依存する(MapEditor.cpp/TerrainFactory.cppは
// ComponentTypes.h越しにしかコンポーネントの詳細を知らない、という依存の切り方にしている)
#include "../../Core/GameObject.h"
#include "../../Components/Render/ModelRenderComponent.h"
#include "../../Components/Animation/SkeletonComponent.h"
#include "../../Components/Collision/ColliderComponent.h"
#include "../../Components/Movement/FollowCameraComponent.h"

namespace
{
	// nlohmann::jsonの配列[x,y,z]をVector3として安全に取り出す(キー欠落時はdefで補う)
	Math::Vector3 GetVec3(const nlohmann::json& params, const char* key, const Math::Vector3& def)
	{
		if (!params.contains(key)) return def;
		auto& v = params[key];
		if (!v.is_array() || v.size() < 3) return def;
		return Math::Vector3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
	}

	nlohmann::json ToJson(const Math::Vector3& v)
	{
		return { v.x, v.y, v.z };
	}
}

void RegisterMapComponentTypes()
{
	static bool s_registered = false;
	if (s_registered) return;	// MapEditor起動時・ゲーム起動時の両方から呼ばれても2重登録しない
	s_registered = true;

	auto& reg = ComponentRegistry::Instance();

	// ///// ///// ///// ///// ///// ///// ///// /////
	// ModelRender：見た目(モデル)を表示するだけのコンポーネント
	// ///// ///// ///// ///// ///// ///// ///// /////
	{
		ComponentTypeInfo info;
		info.typeName = "ModelRender";
		info.defaultParams = { { "model", "" } };
		info.schema = {
			{ "model", "Model Path", ParamType::String },
		};
		info.create = [](GameObject& obj, const nlohmann::json& params)
		{
			auto* model = obj.AddComponent<SkeletonComponent>();
			model->SetModelData(params.value("model", std::string()));
			obj.AddComponent<ModelRenderComponent>();
		};
		reg.Register(std::move(info));
	}

	// ///// ///// ///// ///// ///// ///// ///// /////
	// Collider：直方体の当たり判定(v1はBoxのみ。Cylinder等はshapeパラメータを見て
	//	分岐を増やす形で後から拡張できる)
	// ///// ///// ///// ///// ///// ///// ///// /////
	{
		ComponentTypeInfo info;
		info.typeName = "Collider";
		info.defaultParams = {
			{ "size",   {1.0f, 1.0f, 1.0f} },
			{ "offset", {0.0f, 0.0f, 0.0f} },
		};
		info.schema = {
			{ "size",   "Size",   ParamType::Vector3 },
			{ "offset", "Offset", ParamType::Vector3 },
		};
		info.create = [](GameObject& obj, const nlohmann::json& params)
		{
			Math::Vector3 size = GetVec3(params, "size", { 1,1,1 });
			Math::Vector3 offset = GetVec3(params, "offset", { 0,0,0 });

			auto* collider = obj.AddComponent<ColliderComponent>();
			// TODO: ColliderCategoryをパラメータ化したい場合はParamTypeにEnumを追加して対応する
			collider->AddBox("body", size, offset, ColliderCategory::Ground);
		};
		reg.Register(std::move(info));
	}

	// ///// ///// ///// ///// ///// ///// ///// /////
	// FollowCamera：注視点からのオフセットでカメラを追従させる
	// ///// ///// ///// ///// ///// ///// ///// /////
	{
		ComponentTypeInfo info;
		info.typeName = "FollowCamera";
		info.defaultParams = { { "offset", {0.0f, -25.0f, 0.0f} } };
		info.schema = {
			{ "offset", "Offset", ParamType::Vector3 },
		};
		info.create = [](GameObject& obj, const nlohmann::json& params)
		{
			Math::Vector3 offset = GetVec3(params, "offset", { 0,-25.0f,0 });
			obj.AddComponent<FollowCameraComponent>()->SetOffset(offset);
		};
		reg.Register(std::move(info));
	}

	// 他のコンポーネントを増やす時は、ここに同じ形でブロックを追加するだけでよい。
	// MapEditor.cpp / TerrainFactory.cpp 側の改修は不要
}
