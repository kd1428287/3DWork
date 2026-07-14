#include "CameraFactory.h"
#include "../../Components/Camera/CameraComponent.h"
#include "../../Components/Camera/CameraFollowComponent.h"
#include "../../Components/Camera/CameraViewComponent.h"
#include "../../Components/Camera/CameraOrbitComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"


GameObject* CameraFactory::CreateCamera(ObjectManager& objectManager, GameObject* target, int ownerCameraId)
{
	return CreateCamera(objectManager, target->GetComponent<CameraTargetComponent>(), ownerCameraId);
}

GameObject* CameraFactory::CreateCamera(ObjectManager& objectManager, CameraTargetComponent* target, int ownerCameraId)
{
	GameObject* camera = objectManager.Instantiate("camera");
	camera->AddComponent<TransformComponent>();
	camera->AddComponent<CameraComponent>();
	auto* follow = camera->AddComponent<CameraFollowComponent>();
	camera->AddComponent<CameraOrbitComponent>();
	camera->AddComponent<CameraViewComponent>();
	follow->SetTarget(target);
	follow->SetLocalOffset({ 0.0f, 1.0f, -5.0f });

	return camera;
}

