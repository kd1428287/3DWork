#pragma once

// 初期設定(Prefab/JSONから渡す値)。
struct CharacterBodyParams
{
	float footOffset = 0.0f;
	float height = 2.0f;
	float width = 1.0f;

};


class CharacterBodyComponent : public ComponentBase
{
public:
	explicit CharacterBodyComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;
};