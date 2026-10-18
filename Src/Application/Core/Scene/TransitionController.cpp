#include "TransitionController.h"
#include "Application/main.h"

TransitionController::TransitionController()
{
	m_requestSub = GLOBALEVENT->SubscribeScoped<Events::TransitionRequest>(
		[this](const Events::TransitionRequest& e) { OnRequest(e); });
}

TransitionController::~TransitionController()
{
	Shutdown();
}

void TransitionController::Shutdown()
{
	m_requestSub.reset();
}

void TransitionController::OnRequest(const Events::TransitionRequest& e)
{
	// 遷移中の要求は無視される
	if (m_transition.Start(e.outTime, e.inTime))
	{
		m_center = e.center;
		m_color = e.color;
	}
}

void TransitionController::Update(float deltaTime)
{
	// ロード直後の大きなdtでInが一瞬で終わるのを防ぐ
	const float dt = (std::min)(deltaTime, 1.0f / 30.0f);

	switch (m_transition.Update(dt))
	{
	case SceneTransition::Signal::Covered:
		GLOBALEVENT->Publish(Events::TransitionCovered{});
		break;
	case SceneTransition::Signal::Finished:
		GLOBALEVENT->Publish(Events::TransitionFinished{});
		break;
	default: break;
	}

	KdShaderManager::Instance().m_postProcessShader.SetTransition(
		m_transition.GetProgress(), m_center, m_color, m_transition.GetTime());
}
