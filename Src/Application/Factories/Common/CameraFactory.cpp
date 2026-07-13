#include "CameraFactory.h"
#include "../../Components/Camera/CameraComponent.h"
#include "../../Components/Camera/CameraFollowComponent.h"
#include "../../Components/Camera/CameraViewComponent.h"
#include "../../Components/Camera/CameraTargetComponent.h"

GameObject* CameraFactory::CreateCamera(ObjectManager& objectManager, int ownerCameraId)
{
	// --- カメラ: CameraComponent(カメラであること) + CameraFollowComponent(追従) ---
	GameObject* camera = objectManager.Instantiate("camera");
	camera->AddComponent<TransformComponent>();
	auto* camComp = camera->AddComponent<CameraComponent>();
	auto* follow = camera->AddComponent<CameraFollowComponent>();
	camera->AddComponent<CameraViewComponent>();
	auto player = objectManager.FindName("player");

	follow->SetTarget(player[0]->GetComponent<CameraTargetComponent>());
	follow->SetLocalOffset({ 0.0f, 1.0f, -5.0f });

	// 4. 所有権を持たない「利用権（参照用）」としての生ポインタを返す
	return camera;
}
