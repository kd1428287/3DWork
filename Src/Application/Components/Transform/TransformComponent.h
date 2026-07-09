#pragma once

// ============================================================
// サンプルコンポーネント その1: Transform
// ほぼ全てのGameObjectが持つであろう最も基本的なコンポーネント。
// ============================================================
class TransformComponent : public ComponentBase {
public:
	explicit TransformComponent(GameObject* owner) : ComponentBase(owner) {}

	Math::Vector3    position;
	Math::Quaternion rotation = Math::Quaternion::Identity;
	Math::Vector3    scale{ 1.0f, 1.0f, 1.0f };

	// Matrixは「保持するもの」ではなく「必要な時に都度作るもの」にする
	Math::Matrix GetWorldMatrix() const {
		return Math::Matrix::CreateScale(scale)
			* Math::Matrix::CreateFromQuaternion(rotation)
			* Math::Matrix::CreateTranslation(position);
	}

	Math::Vector3 GetForward() const { return Math::Vector3::Transform(Math::Vector3::Forward, rotation); }
	Math::Vector3 GetUp()      const { return Math::Vector3::Transform(Math::Vector3::Up, rotation); }
	Math::Vector3 GetRight()   const { return Math::Vector3::Transform(Math::Vector3::Right, rotation); }
};