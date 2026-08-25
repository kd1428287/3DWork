#pragma once
#pragma once

struct TransformData {
	Math::Vector3 position = { 0.f, 0.f, 0.f };
	Math::Vector3 rotation = { 0.f, 0.f, 0.f }; // オイラー角(度)
	Math::Vector3 scale = { 1.f, 1.f, 1.f };
};

struct EntityData {
	std::string id;
	std::string type;       // "Environment", "Prop" など
	std::string assetId;    // "model_plane", "model_tree_pine" など
	TransformData transform;
	std::string colliderType; // "Box", "Cylinder" など
};

struct MapData {
	std::string name;
	std::vector<EntityData> entities;
};