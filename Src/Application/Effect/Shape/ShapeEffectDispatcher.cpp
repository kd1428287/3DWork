#include "Application/main.h"
#include "ShapeEffectDispatcher.h"
#include "Framework/Shader/KdShaderManager.h"
#include "Application/Definitions/Effect/EffectParams.h"


// 購読開始
bool ShapeEffectDispatcher::Init(EventBus& bus)
{
	subscriptions_.reserve(3);

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::ShapeEffectAttach>(
			[this](const Events::ShapeEffectAttach& e)
			{
				std::lock_guard<std::mutex> lock(pendingMutex_);
				pendingEvents_.push_back(PendingAttach{ e.InstanceKey, e.EffectName, e.Base, e.MaxDuration });
			}));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::ShapeEffectBaseUpdate>(
			[this](const Events::ShapeEffectBaseUpdate& e)
			{
				std::lock_guard<std::mutex> lock(pendingMutex_);
				pendingEvents_.push_back(PendingBaseUpdate{ e.InstanceKey, e.Base });
			}));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::ShapeEffectDetach>(
			[this](const Events::ShapeEffectDetach& e)
			{
				std::lock_guard<std::mutex> lock(pendingMutex_);
				pendingEvents_.push_back(PendingDetach{ e.InstanceKey });
			}));

	return true;
}

void ShapeEffectDispatcher::Release()
{
	subscriptions_.clear();

	{
		std::lock_guard<std::mutex> lock(pendingMutex_);
		pendingEvents_.clear();
	}

	active_.clear();
	definitions_.clear();
}

void ShapeEffectDispatcher::RegisterDefinition(const std::string& name, const ShapeEffectDefinition& def)
{
	definitions_[name] = def;
}

// pendingを取り出して1件ずつ処理する
void ShapeEffectDispatcher::ProcessPendingEvents()
{
	std::vector<PendingEvent> events;
	{
		std::lock_guard<std::mutex> lock(pendingMutex_);
		events.swap(pendingEvents_);
	}

	for (const PendingEvent& ev : events)
	{
		std::visit([this](const auto& e)
			{
				using T = std::decay_t<decltype(e)>;

				if constexpr (std::is_same_v<T, PendingAttach>)
				{
					auto def = definitions_.find(e.EffectName);
					if (def == definitions_.end()) { return; }

					// 同じキーが既にあれば置き換える
					Active active;
					active.instance.Init(def->second.Motion, def->second.Params, &textureProvider_);
					active.base = e.Base;
					active.maxDurationRemaining = (e.MaxDuration > 0.0f) ? e.MaxDuration : -1.0f;
					active_[e.InstanceKey] = std::move(active);
				}
				else if constexpr (std::is_same_v<T, PendingBaseUpdate>)
				{
					auto it = active_.find(e.InstanceKey);
					if (it != active_.end()) { it->second.base = e.Base; }
				}
				else if constexpr (std::is_same_v<T, PendingDetach>)
				{
					active_.erase(e.InstanceKey);
				}
			}, ev);
	}
}

void ShapeEffectDispatcher::Update(float deltaTime)
{
	ProcessPendingEvents();

	for (auto it = active_.begin(); it != active_.end(); )
	{
		Active& active = it->second;
		active.instance.Update(deltaTime);

		bool dead = active.instance.IsFinished();

		if (active.maxDurationRemaining >= 0.0f)
		{
			active.maxDurationRemaining -= deltaTime;
			if (active.maxDurationRemaining <= 0.0f) { dead = true; }
		}

		if (dead) { it = active_.erase(it); }
		else { ++it; }
	}
}

void ShapeEffectDispatcher::Draw(ParticleDrawPass pass)
{
	if (active_.empty()) { return; }

	// Defaultパスはエフェクト層へ描く(EffectDispatcherと同じ)
	KdPostProcessShader& postProcess = KdShaderManager::Instance().m_postProcessShader;
	const bool useEffectLayer = (pass == ParticleDrawPass::Default);
	if (useEffectLayer) { postProcess.BeginEffectLayer(); }

	for (auto& pair : active_)
	{
		const Active& active = pair.second;
		if (active.instance.GetParams().DrawPass != pass) { continue; }

		active.instance.Draw(active.base);
	}

	if (useEffectLayer) { postProcess.EndEffectLayer(); }
}
