#pragma once

#include <algorithm>
#include <cmath>

// 遷移の進行管理(描画・通知は持たない)。Out→Hold→In の順に進む
class SceneTransition
{
public:
	enum class Phase { Idle, Out, Hold, In };

	// Update()が返す節目の通知
	enum class Signal { None, Covered, Finished };

	// 遷移中の再開始は無視してfalseを返す
	bool Start(float outTime, float inTime)
	{
		if (m_phase != Phase::Idle) { return false; }

		m_outTime = (std::max)(outTime, 0.01f);
		m_inTime = (std::max)(inTime, 0.01f);
		m_timer = 0.0f;
		m_progress = 0.0f;
		m_phase = Phase::Out;
		return true;
	}

	// 覆い切ったフレームを描画した次のUpdateでCoveredを返す
	Signal Update(float dt)
	{
		m_time = std::fmod(m_time + dt, 100.0f);

		switch (m_phase)
		{
		case Phase::Out:
		{
			m_timer += dt;
			float t = (std::min)(m_timer / m_outTime, 1.0f);
			m_progress = Smooth(t);
			if (t >= 1.0f) { m_progress = 1.0f; m_phase = Phase::Hold; }
			break;
		}
		case Phase::Hold:
			m_timer = 0.0f;
			m_phase = Phase::In;
			return Signal::Covered;

		case Phase::In:
		{
			m_timer += dt;
			float t = (std::min)(m_timer / m_inTime, 1.0f);
			m_progress = 1.0f - Smooth(t);
			if (t >= 1.0f) { m_progress = 0.0f; m_phase = Phase::Idle; return Signal::Finished; }
			break;
		}
		default: break;
		}

		return Signal::None;
	}

	bool  IsActive() const { return m_phase != Phase::Idle; }
	float GetProgress() const { return m_progress; }
	float GetTime() const { return m_time; }

private:
	static float Smooth(float t) { return t * t * (3.0f - 2.0f * t); }

	Phase m_phase = Phase::Idle;
	float m_timer = 0.0f;
	float m_outTime = 0.5f;
	float m_inTime = 0.5f;
	float m_progress = 0.0f;	// 0=通常 1=完全に覆われた
	float m_time = 0.0f;		// 波のアニメーション用
};
