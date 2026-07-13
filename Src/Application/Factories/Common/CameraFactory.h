#pragma once
#include <memory>
#include <string>

// 前方宣言でコンパイル時間を短縮
class GameObject;
class ObjectManager;

class CameraFactory {
public:
	CameraFactory() = default;
	~CameraFactory() = default;

	// コピー・ムーブ禁止
	CameraFactory(const CameraFactory&) = delete;
	CameraFactory& operator=(const CameraFactory&) = delete;

	/**
	 * @brief プレイヤーキャラクターを生成し、必要なコンポーネントをアタッチして返す
	 * @param objectManager オブジェクト管理システム
	 * @param ownerCameraId マルチプレイ等を見据えたプレイヤー識別ID（不要なら削除可）
	 * @return 生成されたGameObjectのスマートポインタ
	 */
	GameObject* CreateCamera(ObjectManager& objectManager, int ownerCameraId = 0);
};