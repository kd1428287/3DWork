#pragma once

#include "SceneTransition.h"
#include "Application/Core/EventBus/EventBus.h"	// ScopedSubscriberを値メンバで持つため
#include "Application/Core/EventBus/Events/TransitionEvents.h"

// TransitionRequestを購読して遷移を駆動し、節目をイベントで通知する
class TransitionController
{
public:

	// 毎フレームSceneManager::Update()より前に呼ぶ
	void Update(float deltaTime);

	bool  IsActive() const { return m_transition.IsActive(); }
	float GetProgress() const { return m_transition.GetProgress(); }

	// エンジン系シングルトンの破棄より前に呼ぶ(複数回呼んでも安全)
	void Shutdown();

private:

	TransitionController();
	~TransitionController();

	void OnRequest(const Events::TransitionRequest& e);

	SceneTransition m_transition;
	Math::Vector2 m_center = { 0.5f, 0.5f };
	Math::Vector3 m_color = { 0.0f, 0.0f, 0.0f };

	ScopedSubscriber m_requestSub;

public:

	TransitionController(const TransitionController&) = delete;
	TransitionController& operator=(const TransitionController&) = delete;

	static TransitionController& Instance()
	{
		static TransitionController instance;
		return instance;
	}
};
