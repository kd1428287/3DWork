#pragma once

#include "Application/Systems/SystemManager.h"

class BaseScene
{
public:

	BaseScene();
	virtual ~BaseScene();

	// 生成直後にSceneManagerが1度だけ呼ぶ。共通基盤を作ってからOnEnter()を呼ぶ
	void Enter();

	// 破棄前にSceneManagerが呼ぶ。OnExit()を呼ぶだけで、基盤の破棄はデストラクタに任せる
	void Exit();

	// 全シーン共通の更新処理。
	// systemManager_->Update()を必ず呼ぶ「唯一の入り口」なので、
	// 派生クラスでのオーバーライドは許可しない（呼び忘れを構造的に防ぐため）。
	void Update(float deltaTime);

	void PreDraw(float deltaTime);
	void Draw();
	void DrawSprite();
	void DrawDebug();

protected:

	// シーン固有の初期化はこちらに書く(基盤は構築済みの状態で呼ばれる)
	virtual void OnEnter() = 0;

	// シーン固有の終了処理(基盤は生きている)。メンバの手動解放は不要
	virtual void OnExit() {}

	// シーン固有の更新処理はこちらをオーバーライドする
	// (systemManager_->Update()の後に呼ばれる)
	virtual void OnUpdate(float deltaTime) {}

	virtual void OnPreDraw(float deltaTime) {}
	virtual void OnDrawEffects() {}
	virtual void OnDrawBright() {}
	virtual void OnDrawLiquid() {}
	virtual void OnDrawSprite() {}

	bool LoadUI(const std::string& path);

	// 宣言順の逆に破棄される(派生メンバ → systemManager_ → objManager_ → localBus_)
	std::unique_ptr<EventBus> localBus_ = nullptr;
	std::unique_ptr<ObjectManager> objManager_ = nullptr;
	std::unique_ptr<SystemManager> systemManager_ = nullptr;

private:

	bool entered_ = false;
	bool exited_ = false;
};