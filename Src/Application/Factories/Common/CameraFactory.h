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

	// targetがnullptrやCameraTargetComponentを持たない場合は、追従対象なしで生成する。
	GameObject* CreateCamera(ObjectManager& objectManager, GameObject* target = nullptr, int ownerCameraId = 0);
	// 既定引数を持たせない(GameObject*版との呼び分けがあいまいになるため)。
	GameObject* CreateCamera(ObjectManager& objectManager, CameraTargetComponent* target, int ownerCameraId = 0);
};
