#pragma once

struct TransformComponent : public ComponentData<TransformComponent>
{
	Math::Vector3 pos;
	Math::Vector3 rot;
	Math::Vector3 scale;
};