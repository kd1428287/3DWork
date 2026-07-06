#pragma once
#include "../Tags/CameraTarget.h"

class CameraComponent : public ComponentBase, public IRenderable
{
public:
	CameraComponent(GameObject* owner)
		: ComponentBase(owner) {}

	void PostUpdate(float dt)override
	{
		Math::Vector3 targetPos = Math::Vector3::Zero;
		auto tagged = GetOwner()->GetTagged<CameraTarget>();
		if (tagged.size() == 1)
		{
			targetPos = tagged.at(0)->GetPos();
		}

		m_mWorld =
			Math::Matrix::CreateFromYawPitchRoll(Math::Vector3::Zero) *
			m_mLocalPos *
			Math::Matrix::CreateTranslation(targetPos);
	}

	void PreDraw()override
	{
		if (!m_spCamera) { return; }

		m_spCamera->SetCameraMatrix(m_mWorld);
		m_spCamera->SetToShader();
	}

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

	std::shared_ptr<KdCamera>					m_spCamera = nullptr;
	std::vector<std::weak_ptr<KdGameObject>>	m_wpHitObjectList{};

	Math::Matrix								m_mLocalPos = Math::Matrix::Identity;
	Math::Matrix								m_mRotation = Math::Matrix::Identity;
	Math::Matrix								m_mWorld	= Math::Matrix::Identity;
};