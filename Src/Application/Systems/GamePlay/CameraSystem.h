#pragma once
#include "Application/Components/GamePlay/Camera/CameraComponent.h"
#include "Application/Core/EventBus/Events/CameraEvents.h"

// カメラの切り替え・シェイクなどを担うシステム。
class CameraSystem {
public:
    CameraSystem(ObjectManager& objManager) : objManager_(objManager) {
		subscriptions_.push_back(ScopedSubscriber(objManager.GetSceneEventBus(),
			objManager.GetSceneEventBus()->Subscribe<Events::CameraShakeEvent>(
				[this](const Events::CameraShakeEvent& e) { OnCameraShake(e); })));

		subscriptions_.push_back(ScopedSubscriber(objManager.GetSceneEventBus(),
			objManager.GetSceneEventBus()->Subscribe<Events::CameraShakeSettingEvent>(
				[this](const Events::CameraShakeSettingEvent& e) { OnCameraShakeSettings(e); })));
    }

    // 明示的にアクティブカメラを切り替える。
    // (CameraComponent::Start()での無条件上書きに頼らず、
    //  複数カメラを扱う場合はここを通す運用にする)
	void SwitchTo(CameraComponent* camera) { objManager_.SetActiveCamera(camera);  }

private:
	void OnCameraShake(const Events::CameraShakeEvent& e)
	{
		CameraComponent* camera = objManager_.GetActiveCamera();
		if (camera == nullptr) return;

		CameraShakeComponent* shake = camera->GetOwner()->GetComponent<CameraShakeComponent>();
		if (shake != nullptr) shake->AddTrauma(e.trauma);
	}

	void OnCameraShakeSettings(const Events::CameraShakeSettingEvent e)
	{
		CameraComponent* camera = objManager_.GetActiveCamera();
		if (camera == nullptr) return;

		CameraShakeComponent* shake = camera->GetOwner()->GetComponent<CameraShakeComponent>();
		if (shake != nullptr)
		{
			if (e.perSecond >= 0)shake->SetTraumaDecay(e.perSecond);
			if (e.frequency >= 0)shake->SetNoiseFrequency(e.frequency);
			if (e.posAmplitude != Math::Vector3(0, 0, 0))shake->SetPositionAmplitude(e.posAmplitude);
			if (e.rotAmplitude != Math::Vector3(0, 0, 0))shake->SetRotationAmplitude(e.rotAmplitude);
		}
	}

	ObjectManager& objManager_;
 
	std::vector<ScopedSubscriber> subscriptions_;
};
