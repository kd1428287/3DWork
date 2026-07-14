#pragma once

class GameObject;
class ObjectManager;
class CameraTargetComponent;

class CameraFactory {
public:
	CameraFactory() = default;
	~CameraFactory() = default;

	// コピー・ムーブ禁止
	CameraFactory(const CameraFactory&) = delete;
	CameraFactory& operator=(const CameraFactory&) = delete;

	GameObject* CreateCamera(ObjectManager& objectManager, GameObject* target = nullptr, int ownerCameraId = 0);
	GameObject* CreateCamera(ObjectManager& objectManager, CameraTargetComponent* target = nullptr,int ownerCameraId = 0);
	
};