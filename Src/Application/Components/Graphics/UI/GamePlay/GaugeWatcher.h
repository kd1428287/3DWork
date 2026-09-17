#pragma once
#include "Application/Core/EventBus/Events/HealthEvents.h"
#include "Application/Core/EventBus/Events/PostureEvents.h"

class GameObject;

// ============================================================
// GaugeWatcher
//
// 指定した対象(target)のHealthChangedEvent/PostureChangedEvent/
// HealthDiedEventをSceneContext::eventBus経由で購読し、最新の値を
// 保持するだけの小さなヘルパー。ComponentBaseは継承しない
// (深い継承ベースクラスを作ると1機能のために肥大化しやすいため)。
// PlayerHudGaugeComponent/EnemyWorldGaugeComponentがメンバとして
// 1つ持つ(コンポジション)形で使う。
//
// SceneContext::eventBus(シーン全体のバス)を使う理由は
// HealthEvents.h冒頭のコメント参照。対象の破棄タイミングに関わらず、
// 保持しているScopedSubscriberによる購読解除が常に安全に行える。
// ============================================================
class GaugeWatcher
{
public:
	// bus			… SceneContext::eventBus
	// target		… 監視したい対象
	// onDied		… 対象死亡時に呼びたい処理(不要ならnullptrのまま)
	void Init(EventBus* bus, Handle<GameObject> target, std::function<void()> onDied = nullptr)
	{
		target_ = target;
		onDied_ = std::move(onDied);

		healthSub_ = ScopedSubscriber(bus, bus->Subscribe<HealthChangedEvent>(
			[this](const HealthChangedEvent& e) {
				if (e.source != target_) return;
				healthRatio_ = e.ratio;
			}));

		postureSub_ = ScopedSubscriber(bus, bus->Subscribe<PostureChangedEvent>(
			[this](const PostureChangedEvent& e) {
				if (e.source != target_) return;
				postureRatio_ = e.ratio;
			}));

		diedSub_ = ScopedSubscriber(bus, bus->Subscribe<HealthDiedEvent>(
			[this](const HealthDiedEvent& e) {
				if (e.source != target_) return;
				if (onDied_) onDied_();
			}));
	}

	float GetHealthRatio() const { return healthRatio_; }
	float GetPostureRatio() const { return postureRatio_; }
	Handle<GameObject> GetTarget() const { return target_; }

private:
	Handle<GameObject> target_;
	float healthRatio_ = 1.0f;
	float postureRatio_ = 0.0f;

	std::function<void()> onDied_;

	ScopedSubscriber healthSub_;
	ScopedSubscriber postureSub_;
	ScopedSubscriber diedSub_;
};
