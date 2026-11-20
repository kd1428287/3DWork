#pragma once

#include "Application/Components/Graphics/UI/UITransformComponent.h"
#include "Application/Components/Graphics/UI/Text/TextComponent.h"

#include "Application/Definitions/Json/KdJson.h"

// UITransformComponent::Space: JSONでは"Screen"/"World"で指定する。未知の値はScreenになる。
NLOHMANN_JSON_SERIALIZE_ENUM(UITransformComponent::Space, {
	{ UITransformComponent::Space::Screen, "Screen" },
	{ UITransformComponent::Space::World, "World" },
	})

// TextComponent::Align: JSONでは"Left"/"Center"/"Right"で指定する。未知の値はLeftになる。
NLOHMANN_JSON_SERIALIZE_ENUM(TextComponent::Align, {
	{ TextComponent::Align::Left, "Left" },
	{ TextComponent::Align::Center, "Center" },
	{ TextComponent::Align::Right, "Right" },
	})
