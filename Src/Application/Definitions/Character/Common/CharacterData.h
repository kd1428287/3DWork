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
	std::vector<ComponentEntry>	components;
	std::vector<CharacterNode>	children;
	nlohmann::json				extra = nlohmann::json::object();	// name/components/children以外のキーをそのまま保持
};

struct CharacterFile
{
	CharacterNode	root;
	NodeId			nextId = 1;
};

// 1キャラクター=1JSON。失敗時はfalse
bool LoadCharacterFile(const std::string& path, CharacterFile& out);
bool SaveCharacterFile(const std::string& path, const CharacterFile& file);

// node以下の全ノードに新しいIDを振り直す(複製用)
void ReassignNodeIds(CharacterNode& node, NodeId& nextId);
