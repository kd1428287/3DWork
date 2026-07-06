#pragma once

class CameraRenderComponent : public ComponentBase, public IRenderable
{
public:
	void PreDraw()override
	{
		if (!m_spCamera) { return; }

		m_spCamera->SetCameraRenderMatrix(m_mWorld);
		m_spCamera->SetToShader();
	}


	void SetTarget(const std::shared_ptr<KdGameObject>& target);

	const Math::Matrix GetRotationMatrix()const
	{
		return Math::Matrix::CreateFromYawPitchRoll(
			DirectX::XMConvertToRadians(m_DegAng.y),
			DirectX::XMConvertToRadians(m_DegAng.x),
			DirectX::XMConvertToRadians(m_DegAng.z));
	}

	const Math::Matrix GetRotationYMatrix() const
	{
		return Math::Matrix::CreateRotationY(
			DirectX::XMConvertToRadians(m_DegAng.y));
	}

	void RegistHitObject(const std::shared_ptr<KdGameObject>& object)
	{
		m_wpHitObjectList.push_back(object);
	}

protected:
	// カメラ回転用角度
	Math::Vector3								m_DegAng = Math::Vector3::Zero;

	void UpdateRotateByMouse();

	std::shared_ptr<KdCamera>					m_spCamera = nullptr;
	std::weak_ptr<KdGameObject>					m_wpTarget;
	std::vector<std::weak_ptr<KdGameObject>>	m_wpHitObjectList{};

	Math::Matrix								m_mLocalPos = Math::Matrix::Identity;
	Math::Matrix								m_mRotation = Math::Matrix::Identity;

	// カメラ回転用マウス座標の差分
	POINT										m_FixMousePos{};
};
};