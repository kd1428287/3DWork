#pragma once

#include "../../Engine/SystemManager/SystemManager.h"

class BaseScene
{
public:

	BaseScene();
	virtual ~BaseScene();

	// 全シーン共通の更新処理。
	// systemManager_->Update()を必ず呼ぶ「唯一の入り口」なので、
	// 派生クラスでのオーバーライドは許可しない（呼び忘れを構造的に防ぐため）。
	void Update(float deltaTime);

	void PreDraw(float deltaTime);
	void Draw();
	void DrawSprite();
	void DrawDebug();

protected:

	// シーン固有の更新処理はこちらをオーバーライドする
	// (systemManager_->Update()の後に呼ばれる)
	virtual void OnUpdate(float deltaTime) {}

	virtual void Init();

	std::unique_ptr<SystemManager> systemManager_ = nullptr;

	std::unique_ptr<EventBus> localBus_;
	std::unique_ptr<ObjectManager> objManager_ = nullptr;
};