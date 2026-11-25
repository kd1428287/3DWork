#include "Application/main.h"

#include "EditorFileUI.h"

#include <algorithm>
#include <filesystem>

JsonFileList::JsonFileList(const char* defaultDir, Filter filter)
	: m_filter(std::move(filter))
{
	strncpy_s(m_dirBuf, defaultDir, _TRUNCATE);
}

void JsonFileList::Refresh()
{
	namespace fs = std::filesystem;

	m_entries.clear();
	m_loaded = true;

	const fs::path dir(m_dirBuf);
	std::error_code ec;
	for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
	{
		if (!it->is_regular_file() || it->path().extension() != ".json") continue;

		const std::string path = it->path().generic_string();
		nlohmann::json j;
		if (!JsonLoader::Load(path, j) || !m_filter(j)) continue;

		// 表示名は指定フォルダからの相対パス(拡張子なし)
		fs::path label = it->path().lexically_relative(dir);
		label.replace_extension();
		m_entries.push_back({ label.generic_string(), path });
	}

	std::sort(m_entries.begin(), m_entries.end(),
		[](const Entry& a, const Entry& b) { return a.label < b.label; });
}

bool JsonFileList::Draw(const std::string& currentPath, std::string& outPath, float height)
{
	ImGui::PushID(this);

	ImGui::InputText("Dir", m_dirBuf, sizeof(m_dirBuf));
	if (ImGui::IsItemDeactivatedAfterEdit()) { Invalidate(); }
	if (ImGui::Button("Refresh") || !m_loaded) { Refresh(); }

	ImGui::Separator();

	const bool useChild = (height > 0.0f);
	if (useChild) { ImGui::BeginChild("files", ImVec2(0.0f, height), true); }

	if (m_entries.empty()) { ImGui::TextDisabled("対象のJSONが見つかりません"); }

	bool clicked = false;
	for (const Entry& e : m_entries)
	{
		if (ImGui::Selectable(e.label.c_str(), e.path == currentPath))
		{
			outPath = e.path;
			clicked = true;
		}
	}

	if (useChild) { ImGui::EndChild(); }

	ImGui::PopID();
	return clicked;
}

namespace EditorFileUI
{
	UnsavedChoice DrawUnsavedPopup(bool& openRequested, bool canSave)
	{
		if (openRequested)
		{
			ImGui::OpenPopup("Unsaved Changes");
			openRequested = false;
		}

		UnsavedChoice choice = UnsavedChoice::None;
		if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("未保存の変更があります。破棄して続行しますか？");

			ImGui::BeginDisabled(!canSave);
			if (ImGui::Button("Save & Continue")) { choice = UnsavedChoice::Save; }
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Discard & Continue")) { choice = UnsavedChoice::Discard; }
			ImGui::SameLine();
			if (ImGui::Button("Cancel") || choice != UnsavedChoice::None) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
		return choice;
	}
}
