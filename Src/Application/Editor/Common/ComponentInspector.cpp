#include "Application/main.h"

#include "ComponentInspector.h"

#include "Application/Factories/ComponentRegistry.h"
#include "Application/Definitions/Physics/ColliderCategoryNames.h"
#include "nlohmann/json.hpp"

#include <algorithm>

namespace
{
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// コンポーネント1個ぶんのparamsを、型の初期値(defaults)のJSON型に従って自動描画する汎用UI。
	//	bool→チェックボックス / 数値→ドラッグ / [x,y,z]→3要素ドラッグ / 文字列→入力欄 /
	//	文字列の配列→リスト / オブジェクト→折りたたみ(再帰)。それ以外はJSONの直接編集に任せる。
	//	戻り値は「このフレームで何か編集されたか」(文字列は入力の確定時のみtrue)
	//
	//	defaultsだけnlohmann::ordered_json(挿入順を保持するjson)で受け取っている。
	//	ComponentRegistry::FindDefaultParams()がordered_jsonを返すのに合わせたもので、
	//	これによりメンバの宣言順=このUIの描画順になる(paramsの方は保存内容そのものであり、
	//	キーの並び順は表示に影響しないので従来通りnlohmann::jsonのまま)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	template <typename BasicJsonType>
	bool IsNumberArray(const BasicJsonType& j, size_t count)
	{
		if (!j.is_array() || j.size() != count) return false;
		for (const auto& v : j) { if (!v.is_number()) return false; }
		return true;
	}

	template <typename BasicJsonType>
	bool IsStringArray(const BasicJsonType& j)
	{
		if (!j.is_array()) return false;
		for (const auto& v : j) { if (!v.is_string()) return false; }
		return true;
	}

	bool DrawParamsJson(nlohmann::json& params, const nlohmann::ordered_json& defaults, const ComponentInspector::RequestUndoFn& pushUndo)
	{
		using nlohmann::json;
		using nlohmann::ordered_json;

		if (!defaults.is_object()) return false;
		if (!params.is_object()) params = json::object();

		bool edited = false;

		for (auto it = defaults.begin(); it != defaults.end(); ++it)
		{
			const std::string& key = it.key();
			const ordered_json& def = it.value();
			// defはordered_json、params[key]はjsonなので、無い場合はjsonへ変換して揃える
			const json current = params.contains(key) ? params[key] : json(def);

			ImGui::PushID(key.c_str());

			if (def.is_boolean())
			{
				bool b = current.is_boolean() ? current.get<bool>() : def.get<bool>();
				ImGui::Checkbox(key.c_str(), &b);
				if (ImGui::IsItemActivated()) { pushUndo(); }
				if (ImGui::IsItemEdited()) { params[key] = b; edited = true; }
			}
			else if (def.is_number_integer())
			{
				int v = current.is_number() ? current.get<int>() : def.get<int>();
				ImGui::DragInt(key.c_str(), &v);
				if (ImGui::IsItemActivated()) { pushUndo(); }
				if (ImGui::IsItemEdited()) { params[key] = v; edited = true; }
			}
			else if (def.is_number())
			{
				float v = current.is_number() ? current.get<float>() : def.get<float>();
				ImGui::DragFloat(key.c_str(), &v, 0.1f);
				if (ImGui::IsItemActivated()) { pushUndo(); }
				if (ImGui::IsItemEdited()) { params[key] = v; edited = true; }
			}
			else if (def.is_string())
			{
				const std::string s = current.is_string() ? current.get<std::string>() : def.get<std::string>();
				char buf[256];
				strncpy_s(buf, s.c_str(), _TRUNCATE);
				ImGui::InputText(key.c_str(), buf, sizeof(buf));
				if (ImGui::IsItemActivated()) { pushUndo(); }
				if (ImGui::IsItemEdited()) { params[key] = std::string(buf); }
				// モデルの再読み込み等を入力のたびに走らせないよう、確定時にだけ「編集された」とする
				if (ImGui::IsItemDeactivatedAfterEdit()) { edited = true; }
			}
			else if (IsNumberArray(def, 3))
			{
				const json src = IsNumberArray(current, 3) ? current : json(def);
				float v[3] = { src[0].get<float>(), src[1].get<float>(), src[2].get<float>() };
				ImGui::DragFloat3(key.c_str(), v, 0.1f);
				if (ImGui::IsItemActivated()) { pushUndo(); }
				if (ImGui::IsItemEdited()) { params[key] = json::array({ v[0], v[1], v[2] }); edited = true; }
			}
			else if (IsStringArray(def) && (!params.contains(key) || IsStringArray(params[key])))
			{
				// 空の配列も文字列のリストとして扱う
				if (ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					json list = params.contains(key) ? params[key] : json(def);
					const json before = list;
					int removeIndex = -1;

					for (int i = 0; i < (int)list.size(); i++)
					{
						ImGui::PushID(i);
						const std::string s = list[i].get<std::string>();
						char buf[256];
						strncpy_s(buf, s.c_str(), _TRUNCATE);
						ImGui::InputText("##item", buf, sizeof(buf));
						if (ImGui::IsItemActivated()) { pushUndo(); }
						if (ImGui::IsItemEdited()) { list[i] = std::string(buf); }
						ImGui::SameLine();
						if (ImGui::SmallButton("-")) { removeIndex = i; }
						ImGui::PopID();
					}

					if (removeIndex >= 0) { pushUndo(); list.erase(list.begin() + removeIndex); }
					if (ImGui::SmallButton("+ Add")) { pushUndo(); list.push_back(std::string()); }

					if (list != before) { params[key] = list; edited = true; }
					ImGui::TreePop();
				}
			}
			else if (def.is_object())
			{
				if (ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					json child = params.contains(key) ? params[key] : json::object();
					const json before = child;
					const bool childEdited = DrawParamsJson(child, def, pushUndo);
					if (child != before) { params[key] = child; }
					edited = edited || childEdited;
					ImGui::TreePop();
				}
			}
			else
			{
				ImGui::TextDisabled("%s: (JSONを直接編集)", key.c_str());
			}

			ImGui::PopID();
		}

		return edited;
	}

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// Collider専用のInspector UI。
	//	実際のColliderComponentは「名前付きの複数形状(Sphere/Box/Capsule)」を持てる設計のため、
	//	他のコンポーネントのような汎用UI(DrawParamsJson)では表現できない。
	//	FindCustomInspector()経由でCollider種類にだけ使う
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	bool DrawColliderInspector(nlohmann::json& params, const ComponentInspector::RequestUndoFn& requestUndoCheckpoint)
	{
		bool edited = false;

		{
			// wireFrameがtrueなら、デバッグ用のWireFrameも一緒に付く
			bool wireFrame = params.value("wireFrame", true);
			ImGui::Checkbox("Wire Frame", &wireFrame);
			if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
			if (ImGui::IsItemEdited()) { params["wireFrame"] = wireFrame; edited = true; }
		}

		if (!params.contains("shapes") || !params["shapes"].is_array())
		{
			params["shapes"] = nlohmann::json::array();
		}
		auto& shapes = params["shapes"];

		static const char* kShapeNames[] = { "Box", "Sphere", "Capsule" };
		int removeIndex = -1;

		for (int i = 0; i < (int)shapes.size(); i++)
		{
			auto& shape = shapes[i];
			ImGui::PushID(i);

			std::string name = shape.value("name", std::string("shape"));
			std::string headerLabel = name + "##shapehdr";
			bool open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

			if (open)
			{
				ImGui::Indent();

				// Removeはヘッダーと同じ行に置かず、開いた中身の先頭に置く
				// (理由はDrawList()の同種の修正と同じ)
				if (ImGui::SmallButton("Remove Shape")) { removeIndex = i; }
				ImGui::Separator();

				char nameBuf[64];
				strcpy_s(nameBuf, name.c_str());
				ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
				if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
				if (ImGui::IsItemEdited()) { shape["name"] = std::string(nameBuf); edited = true; }

				std::string shapeType = shape.value("shape", std::string("Box"));
				int currentIdx = 0;
				for (int k = 0; k < 3; k++) { if (shapeType == kShapeNames[k]) { currentIdx = k; break; } }
				if (ImGui::Combo("Shape", &currentIdx, kShapeNames, 3))
				{
					requestUndoCheckpoint();
					shapeType = kShapeNames[currentIdx];
					shape["shape"] = shapeType;
					edited = true;
				}

				{
					auto arr = shape.value("offset", std::vector<float>{0.0f, 0.0f, 0.0f});
					while (arr.size() < 3) { arr.push_back(0.0f); }
					float v[3] = { arr[0], arr[1], arr[2] };
					ImGui::DragFloat3("Offset", v, 0.1f);
					if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
					if (ImGui::IsItemEdited()) { shape["offset"] = { v[0], v[1], v[2] }; edited = true; }
				}

				if (shapeType == "Sphere")
				{
					float radius = shape.value("radius", 0.5f);
					ImGui::DragFloat("Radius", &radius, 0.05f, 0.01f, 100.0f);
					if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
					if (ImGui::IsItemEdited()) { shape["radius"] = radius; edited = true; }
				}
				else if (shapeType == "Capsule")
				{
					float radius = shape.value("radius", 0.5f);
					ImGui::DragFloat("Radius", &radius, 0.05f, 0.01f, 100.0f);
					if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
					if (ImGui::IsItemEdited()) { shape["radius"] = radius; edited = true; }

					auto endArr = shape.value("capsuleEnd", std::vector<float>{0.0f, 1.0f, 0.0f});
					while (endArr.size() < 3) { endArr.push_back(0.0f); }
					float endV[3] = { endArr[0], endArr[1], endArr[2] };
					ImGui::DragFloat3("Capsule End", endV, 0.1f);
					if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
					if (ImGui::IsItemEdited()) { shape["capsuleEnd"] = { endV[0], endV[1], endV[2] }; edited = true; }
				}
				else // Box
				{
					auto heArr = shape.value("halfExtents", std::vector<float>{0.5f, 0.5f, 0.5f});
					while (heArr.size() < 3) { heArr.push_back(0.5f); }
					float he[3] = { heArr[0], heArr[1], heArr[2] };
					ImGui::DragFloat3("Half Extents", he, 0.05f, 0.01f, 100.0f);
					if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
					if (ImGui::IsItemEdited()) { shape["halfExtents"] = { he[0], he[1], he[2] }; edited = true; }
				}

				bool enabled = shape.value("enabled", true);
				ImGui::Checkbox("Enabled", &enabled);
				if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
				if (ImGui::IsItemEdited()) { shape["enabled"] = enabled; edited = true; }

				bool isTrigger = shape.value("isTrigger", false);
				ImGui::Checkbox("Is Trigger", &isTrigger);
				if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
				if (ImGui::IsItemEdited()) { shape["isTrigger"] = isTrigger; edited = true; }

				bool isStatic = shape.value("isStatic", false);
				ImGui::Checkbox("Is Static", &isStatic);
				if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
				if (ImGui::IsItemEdited()) { shape["isStatic"] = isStatic; edited = true; }

				bool wantsStay = shape.value("wantsStayEvent", false);
				ImGui::Checkbox("Wants Stay Event", &wantsStay);
				if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
				if (ImGui::IsItemEdited()) { shape["wantsStayEvent"] = wantsStay; edited = true; }

				ImGui::Separator();

				// categoryMask：この形状が「何者であるか」。複数選択可(坂道はGround|Bump等)
				ImGui::Text("Category");
				{
					auto categoryNames = shape.value("categoryMask", std::vector<std::string>{"Bump"});
					bool changed = false;
					int col = 0;
					for (auto& kv : GetColliderCategoryFlags())
					{
						const char* flagName = kv.first;
						bool has = std::find(categoryNames.begin(), categoryNames.end(), std::string(flagName)) != categoryNames.end();
						bool before = has;

						if (col > 0) { ImGui::SameLine(); }
						ImGui::Checkbox(flagName, &has);
						if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }

						if (has != before)
						{
							if (has) { categoryNames.push_back(flagName); }
							else { categoryNames.erase(std::remove(categoryNames.begin(), categoryNames.end(), std::string(flagName)), categoryNames.end()); }
							changed = true;
						}

						col = (col + 1) % 3;
					}
					if (changed) { shape["categoryMask"] = categoryNames; edited = true; }
				}

				// collideMask：この形状が「誰と判定したいか」。既定ではColliderLayerMatrixの
				// デフォルトに任せ、チェックを外した時だけ個別に選べるようにする
				ImGui::Text("Collide With");
				{
					bool useDefault = shape.value("useDefaultCollideMask", true);
					ImGui::Checkbox("Use Layer Matrix Default", &useDefault);
					if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }
					if (ImGui::IsItemEdited()) { shape["useDefaultCollideMask"] = useDefault; edited = true; }

					if (!useDefault)
					{
						auto collideNames = shape.value("collideMask", std::vector<std::string>{});
						bool changed = false;
						int col = 0;
						for (auto& kv : GetColliderCategoryFlags())
						{
							const char* flagName = kv.first;
							bool has = std::find(collideNames.begin(), collideNames.end(), std::string(flagName)) != collideNames.end();
							bool before = has;

							std::string label = std::string(flagName) + "##collide";
							if (col > 0) { ImGui::SameLine(); }
							ImGui::Checkbox(label.c_str(), &has);
							if (ImGui::IsItemActivated()) { requestUndoCheckpoint(); }

							if (has != before)
							{
								if (has) { collideNames.push_back(flagName); }
								else { collideNames.erase(std::remove(collideNames.begin(), collideNames.end(), std::string(flagName)), collideNames.end()); }
								changed = true;
							}

							col = (col + 1) % 3;
						}
						if (changed) { shape["collideMask"] = collideNames; edited = true; }
					}
				}

				ImGui::Unindent();
			}

			ImGui::PopID();
		}

		if (removeIndex >= 0)
		{
			requestUndoCheckpoint();
			shapes.erase(shapes.begin() + removeIndex);
			edited = true;
		}

		if (ImGui::Button("+ Add Shape"))
		{
			requestUndoCheckpoint();

			nlohmann::json newShape;
			newShape["name"] = "shape" + std::to_string(shapes.size());
			newShape["shape"] = "Box";
			newShape["offset"] = { 0.0f, 0.0f, 0.0f };
			newShape["halfExtents"] = { 0.5f, 0.5f, 0.5f };
			newShape["isTrigger"] = false;
			newShape["isStatic"] = false;
			newShape["wantsStayEvent"] = false;
			newShape["categoryMask"] = std::vector<std::string>{ "Bump" };
			newShape["useDefaultCollideMask"] = true;
			shapes.push_back(std::move(newShape));

			edited = true;
		}

		return edited;
	}

	// schemaでは表現できない構造(Colliderの形状リスト等)を持つ種類にだけ用意する専用Inspector
	using CustomInspectorFn = bool (*)(nlohmann::json&, const ComponentInspector::RequestUndoFn&);

	CustomInspectorFn FindCustomInspector(const std::string& type)
	{
		if (type == "Collider") return DrawColliderInspector;
		return nullptr;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// コンポーネント一覧(追加/削除/パラメータ編集)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void ComponentInspector::DrawList(
	std::vector<ComponentEntry>& components,
	const RequestUndoFn& requestUndoCheckpoint,
	const OnEditedFn& onComponentEdited)
{
	ImGui::Text("Components");

	int removeIndex = -1;

	for (int i = 0; i < (int)components.size(); i++)
	{
		ComponentEntry& entry = components[i];
		ImGui::PushID(i);

		bool open = ImGui::CollapsingHeader(entry.type.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		if (open)
		{
			ImGui::Indent();

			// Removeはヘッダーと同じ行に置かず、開いた中身の先頭に置く。
			// (CollapsingHeaderと同じ行にSameLineでボタンを乗せると、ヘッダー側が
			//  クリックを奪ってしまいボタンが押せないことがある為)
			if (ImGui::SmallButton("Remove Component")) { removeIndex = i; }
			ImGui::Separator();

			const nlohmann::ordered_json* defaults = ComponentRegistry::Instance().FindDefaultParams(entry.type);
			if (defaults)
			{
				bool edited = false;

				if (CustomInspectorFn custom = FindCustomInspector(entry.type))
				{
					// 汎用UIでは表現できない構造(Colliderの形状リスト等)を持つコンポーネント
					edited = custom(entry.params, requestUndoCheckpoint);
				}
				else
				{
					edited = DrawParamsJson(entry.params, *defaults, requestUndoCheckpoint);
				}

				if (edited && onComponentEdited)
				{
					onComponentEdited(entry.type);
				}
			}
			else
			{
				ImGui::TextDisabled("未登録のコンポーネント種類です: %s", entry.type.c_str());
			}

			ImGui::Unindent();
		}

		ImGui::PopID();
	}

	if (removeIndex >= 0)
	{
		requestUndoCheckpoint();
		std::string removedType = components[removeIndex].type;
		components.erase(components.begin() + removeIndex);
		// 削除でも呼び出し元に通知する(例:ModelRenderを消したらプレビューも消す)
		if (onComponentEdited) { onComponentEdited(removedType); }
	}

	if (ImGui::Button("+ Add Component"))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		for (const std::string& typeName : ComponentRegistry::Instance().GetTypeNames())
		{
			bool hasAlready = std::any_of(components.begin(), components.end(),
				[&](const ComponentEntry& c) { return c.type == typeName; });
			if (hasAlready) continue;

			if (ImGui::Selectable(typeName.c_str()))
			{
				requestUndoCheckpoint();

				ComponentEntry entry;
				entry.type = typeName;
				if (const nlohmann::ordered_json* defaults = ComponentRegistry::Instance().FindDefaultParams(typeName))
				{
					// entry.paramsは従来通りnlohmann::json(表示順を持つ必要がないため)
					entry.params = nlohmann::json(*defaults);
				}
				components.push_back(std::move(entry));

				if (onComponentEdited) { onComponentEdited(typeName); }
			}
		}
		ImGui::EndPopup();
	}
}