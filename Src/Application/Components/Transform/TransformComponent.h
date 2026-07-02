#pragma once

// ============================================================
// サンプルコンポーネント その1: Transform
// ほぼ全てのGameObjectが持つであろう最も基本的なコンポーネント。
// ============================================================
class TransformComponent : public ComponentBase {
public:
	explicit TransformComponent(GameObject* owner) : ComponentBase(owner) {}

	Math::Vector3 position;
	Math::Vector3 rotation;
	Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
};