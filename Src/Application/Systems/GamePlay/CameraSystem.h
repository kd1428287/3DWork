#pragma once
#include "Application/Components/GamePlay/Camera/CameraComponent.h"
#include "Application/Core/EventBus/Events/CameraEvents.h"

// カメラの切り替え・シェイクなどを担うシステム。
class CameraSystem {
public:
    CameraSystem(ObjectManager& objManager) : objManager_(objManager) {
		auto bus = objManager.GetSceneEventBus();
		subscriptions_.emplace_back(bus->SubscribeScoped<Events::CameraShake>(
			[this](const Events::CameraShake& e) { OnCameraShake(e); }));

		subscriptions_.emplace_back(bus->SubscribeScoped<Events::CameraShakeSetting>(
			[this](const Events::CameraShakeSetting& e) { OnCameraShakeSettings(e); }));
    }

    // 明示的にアクティブカメラを切り替える。
    // (CameraComponent::Start()での無条件上書きに頼らず、
    //  複数カメラを扱う場合はここを通す運用にする)
	void SwitchTo(CameraComponent* camera) { objManager_.SetActiveCamera(camera);  }

private:
	void OnCameraShake(const Events::CameraShake& e)
	{
		CameraComponent* camera = objManager_.GetActiveCamera();
		if (camera == nullptr) return;

		CameraShakeComponent* shake = camera->GetOwner()->GetComponent<CameraShakeComponent>();
		if (shake != nullptr) shake->AddTrauma(e.trauma);
	}

	void OnCameraShakeSettings(const Events::CameraShakeSetting e)
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
