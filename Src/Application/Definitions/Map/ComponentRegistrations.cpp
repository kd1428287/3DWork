#include "ComponentTypes.h"
#include "ColliderCategoryNames.h"

// ここでのみ実際のコンポーネントクラスに依存する(MapEditor.cpp/TerrainFactory.cppは
// ComponentTypes.h越しにしかコンポーネントの詳細を知らない、という依存の切り方にしている)。
// ※ 相対パスはTerrainFactory.cppと同じ階層に置く前提の推測です。実際の配置に合わせて調整してください
#include "Application/Entity/GameObject.h"
#include "Application/Components/Graphics/Render/ModelRenderComponent.h"
#include "Application/Components/Graphics/Animation/SkeletonComponent.h"
#include "Application/Components/Graphics/Render/WireFrameComponent.h"

#include "Application/Components/Physics/Collision/ColliderComponent.h"
#include "Application/Components/Physics/Movement/FollowCameraComponent.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 各コンポーネントのパラメータ構造体。
//	キー名(json上の名前)・ラベル・型は、このFields()の中に1回だけ書く。
//	schema/defaultParamsはComponentTypes.hのRegisterComponent<Struct>()がここから
//	自動生成するので、下のcreateラムダは型安全なメンバアクセスだけで書ける
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

struct ModelRenderParams
{
	std::string model;

	static const std::vector<ReflectedField<ModelRenderParams>>& Fields()
	{
		static std::vector<ReflectedField<ModelRenderParams>> fields = {
			MakeField("model", "Model Path", &ModelRenderParams::model),
		};
		return fields;
	}
};

struct FollowCameraParams
{
	Math::Vector3 offset = { 0.0f, -25.0f, 0.0f };

	static const std::vector<ReflectedField<FollowCameraParams>>& Fields()
	{
		static std::vector<ReflectedField<FollowCameraParams>> fields = {
			MakeField("offset", "Offset", &FollowCameraParams::offset),
		};
		return fields;
	}
};

// jsonの配列[x,y,z]を安全にMath::Vector3として取り出す。
// キー欠落・配列でない・要素数不足の場合はdefを返す。
// (Colliderのcreateは以下の通りRegisterComponent<Struct>()の型安全なパスを使わず、
//  jsonを直接読み書きする昔ながらの形にしているため、ここで改めて必要になる)
static Math::Vector3 GetVec3(const nlohmann::json& j, const char* key, const Math::Vector3& def)
{
	if (!j.contains(key)) return def;
	auto& v = j[key];
	if (!v.is_array() || v.size() < 3) return def;
	return Math::Vector3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Collider：名前付きの複数形状(Sphere/Box/Capsule)を持てる。
//	実際のColliderComponent自体がそういう設計(1コンポーネントの中に複数形状のリスト)なので、
//	他のコンポーネントのようなスカラー値だけのReflectedFieldでは表現できない。
//	そのためこれだけはRegisterComponent<Struct>()の汎用リフレクションを使わず、
//	paramsのjsonを直接読み書きする昔ながらの形で登録する。
//	Inspector側の専用UI(drawCustomInspector)はMapEditor.cpp側から後付けする
//	(ここにImGui依存を持ち込まないため)。
//
//	保存形式:
//	  "params": { "shapes": [
//	      { "name":"body", "shape":"Box", "offset":[0,0,0], "halfExtents":[0.5,0.5,0.5],
//	        "isTrigger":false, "isStatic":false, "wantsStayEvent":false },
//	      { "name":"hurtbox", "shape":"Sphere", "offset":[0,1,0], "radius":0.3, "isTrigger":true }
//	  ] }
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static void RegisterColliderComponent()
{
	ComponentTypeInfo info;
	info.typeName = "Collider";
	info.defaultParams = { { "shapes", nlohmann::json::array() } };
	// schemaは空のまま(自動描画は使わない。drawCustomInspectorはMapEditor.cpp側で後付けする)

	info.create = [](GameObject& obj, const nlohmann::json& params)
		{
			auto* collider = obj.AddComponent<ColliderComponent>();
			obj.AddComponent<WireFrameComponent>();

			if (!params.contains("shapes") || !params["shapes"].is_array()) return;

			for (auto& s : params["shapes"])
			{
				std::string name = s.value("name", std::string("shape"));
				std::string shapeStr = s.value("shape", std::string("Box"));
				Math::Vector3 offset = GetVec3(s, "offset", { 0,0,0 });

				// categoryMaskは常に明示指定(未指定ならBump=AddXxx()自体のデフォルトと同じ)。
				// collideMaskは「レイヤーマトリクスのデフォルトに任せる」がUI上の既定なので、
				// useDefaultCollideMaskがtrueの間はUseMatrixDefaultのまま渡す
				// (AddXxx側でColliderLayerMatrixから解決される)
				ColliderCategory category = ColliderCategoryFromNames(
					s.value("categoryMask", std::vector<std::string>{"Bump"}));

				ColliderCategory collideMask = ColliderCategory::UseMatrixDefault;
				if (!s.value("useDefaultCollideMask", true))
				{
					collideMask = ColliderCategoryFromNames(
						s.value("collideMask", std::vector<std::string>{}));
				}

				CollisionShapeEntry* entry = nullptr;

				if (shapeStr == "Sphere")
				{
					float radius = s.value("radius", 0.5f);
					entry = &collider->AddSphere(name, radius, offset, category, collideMask);
				}
				else if (shapeStr == "Capsule")
				{
					float radius = s.value("radius", 0.5f);
					Math::Vector3 capsuleEnd = GetVec3(s, "capsuleEnd", { 0,1,0 });
					entry = &collider->AddCapsule(name, radius, offset, capsuleEnd, category, collideMask);
				}
				else // "Box"(未知の値もBox扱いにフォールバック)
				{
					Math::Vector3 halfExtents = GetVec3(s, "halfExtents", { 0.5f,0.5f,0.5f });
					entry = &collider->AddBox(name, halfExtents, offset, category, collideMask);
				}

				entry->isTrigger = s.value("isTrigger", false);
				entry->isStatic = s.value("isStatic", false);
				entry->wantsStayEvent = s.value("wantsStayEvent", false);
			}
		};

	ComponentRegistry::Instance().Register(std::move(info));
}

void RegisterMapComponentTypes()
{
	static bool s_registered = false;
	if (s_registered) return;	// MapEditor起動時・ゲーム起動時の両方から呼ばれても2重登録しない
	s_registered = true;

	// ///// ///// ///// ///// ///// ///// ///// /////
	// ModelRender：見た目(モデル)を表示するだけのコンポーネント
	// ///// ///// ///// ///// ///// ///// ///// /////
	RegisterComponent<ModelRenderParams>("ModelRender",
		[](GameObject& obj, const ModelRenderParams& p)
		{
			auto* model = obj.AddComponent<SkeletonComponent>();
			model->SetModelData(p.model);
			obj.AddComponent<ModelRenderComponent>();
		});

	// ///// ///// ///// ///// ///// ///// ///// /////
	// Collider(複数形状対応。詳細は上のRegisterColliderComponent()参照)
	// ///// ///// ///// ///// ///// ///// ///// /////
	RegisterColliderComponent();

	// ///// ///// ///// ///// ///// ///// ///// /////
	// FollowCamera：注視点からのオフセットでカメラを追従させる
	// ///// ///// ///// ///// ///// ///// ///// /////
	RegisterComponent<FollowCameraParams>("FollowCamera",
		[](GameObject& obj, const FollowCameraParams& p)
		{
			obj.AddComponent<FollowCameraComponent>()->SetOffset(p.offset);
		});

	// 他のコンポーネントを増やす時は、パラメータ構造体を1つ書いて
	// RegisterComponent<T>(...)を1回呼ぶだけでよい。
	// MapEditor.cpp / TerrainFactory.cpp 側の改修は不要
}