#include "CameraFactory.h"
#include "Application/Components/GamePlay/Camera/CameraComponent.h"
#include "Application/Components/GamePlay/Camera/CameraFollowComponent.h"
#include "Application/Components/GamePlay/Camera/CameraViewComponent.h"
#include "Application/Components/GamePlay/Camera/CameraOrbitComponent.h"
#include "Application/Components/GamePlay/Camera/CameraTargetComponent.h"
#include "Application/Components/GamePlay/Camera/CameraCollisionComponent.h"
#include "Application/Components/GamePlay/Camera/CameraShakeComponent.h"
#include "Application/Components/GamePlay/Camera/CameraInputCompoent.h"


GameObject* CameraFactory::CreateCamera(ObjectManager& objectManager, GameObject* target, int ownerCameraId)
{
	auto targets = objectManager.FindComponents<CameraTargetComponent>();
	return CreateCamera(objectManager, target->GetComponent<CameraTargetComponent>(), ownerCameraId);
}

GameObject* CameraFactory::CreateCamera(ObjectManager& objectManager, CameraTargetComponent* target, int ownerCameraId)
{
	GameObject* camera = objectManager.Instantiate("camera");
	camera->AddComponent<TransformComponent>();
	auto* cameraC = camera->AddComponent<CameraComponent>();
	auto* follow = camera->AddComponent<CameraFollowComponent>();
	camera->AddComponent<CameraOrbitComponent>();
	camera->AddComponent<CameraViewComponent>();
	follow->SetTarget(Handle<CameraTargetComponent>(target));
	follow->SetDistance(5.f);
	auto* collision = camera->AddComponent<CameraCollisionComponent>(); // Followより後
	collision->SetPivotTarget(Handle<CameraTargetComponent>(target));
	collision->SetPivotOffset(Math::Vector3(0.f, -0.5f, 0.f));;
	collision->SetSweepRadius(0.3f);
	objectManager.SetActiveCamera(cameraC);
	auto* shake = camera->AddComponent<CameraShakeComponent>();
	shake->SetRotationAmplitude({ 0.1f,0.1f,0.15f });
	camera->AddComponent<CameraInputComponent>();

	return camera;
}

