#pragma once

#include "Application/Definitions/Map/MapData.h"	// ComponentEntry
#include "nlohmann/json.hpp"

using NodeId = uint32_t;
constexpr NodeId kInvalidNodeId = 0;

// キャラクターを構成する1ノード(Player.jsonのルート/childrenの1要素に相当)
struct CharacterNode
{
	NodeId						id = kInvalidNodeId;	// 実行時のみ。保存しない
	std::string					name;
	Math::Vector3				pos = Math::Vector3::Zero;
	Math::Quaternion			rotation = Math::Quaternion::Identity;
	Math::Vector3				scale = Math::Vector3::One;
	std::vector<ComponentEntry>	components;
	std::vector<CharacterNode>	children;

	ComponentEntry* FindComponent(std::string_view type) { return ::FindComponent(components, type); }
	const ComponentEntry* FindComponent(std::string_view type) const { return ::FindComponent(components, type); }
};

struct CharacterFile
{
	CharacterNode	root;
	NodeId			nextId = 1;
};

// 1キャラクター=1JSON(実行時と同じPrefabDefinition形式。PrefabIO経由で読み書きする)。失敗時はfalse
bool LoadCharacterFile(const std::string& path, CharacterFile& out);
bool SaveCharacterFile(const std::string& path, const CharacterFile& file);

// 保存されるJSONそのもの。未保存変更の検出(保存済みとの比較)にも使う
nlohmann::json CharacterFileToJson(const CharacterFile& file);

// idでノードを探す(root以下を再帰)。無ければnullptr
CharacterNode* FindNodeById(CharacterNode& root, NodeId id);
// idを子に持つ親を探す(rootのid自身は親なし)
CharacterNode* FindParentOf(CharacterNode& root, NodeId id);

// node以下の全ノードに新しいIDを振り直す(複製用)
void ReassignNodeIds(CharacterNode& node, NodeId& nextId);
