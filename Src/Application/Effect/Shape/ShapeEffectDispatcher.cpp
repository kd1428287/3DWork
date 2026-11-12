#include "Application/main.h"
#include "ShapeEffectDispatcher.h"
#include "Application/Definitions/Loaders/ShapeEffectDataLoader.h"
#include "Framework/Shader/KdShaderManager.h"

std::vector<ShapeEffectDispatcher*> ShapeEffectDispatcher::s_instances;
std::mutex ShapeEffectDispatcher::s_instancesMutex;

// JSON読み込み、購読開始
bool ShapeEffectDispatcher::Init(EventBus& bus, const std::string& dataPath)
{
	dataPath_ = dataPath;
	LoadData(dataPath_);

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

	{
		std::lock_guard<std::mutex> lock(s_instancesMutex);
		s_instances.push_back(this);
	}

	return true;
}

void ShapeEffectDispatcher::Release()
{
	{
		std::lock_guard<std::mutex> lock(s_instancesMutex);
		s_instances.erase(std::remove(s_instances.begin(), s_instances.end(), this), s_instances.end());
	}

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

bool ShapeEffectDispatcher::LoadData(const std::string& path)
{
	ShapeEffectDataFile data;
	if (!ShapeEffectDataLoader::Load(path, data)) { return false; }

	for (const ShapeEffectData& effect : data.Effects)
	{
		auto motion = std::make_shared<KeyframeMotion>();
		motion->keys = effect.Keys;

		ShapeEffectDefinition def;
		def.Params = effect.Params;
		def.Motion = std::move(motion);
		definitions_[effect.Name] = std::move(def);
	}
	return true;
}

void ShapeEffectDispatcher::NotifyDataSaved(const std::string& path)
{
	std::lock_guard<std::mutex> lock(s_instancesMutex);

	for (ShapeEffectDispatcher* dispatcher : s_instances)
	{
		if (dispatcher->dataPath_ == path) { dispatcher->reloadRequested_.store(true, std::memory_order_release); }
	}
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
	// 再生中のインスタンスは旧定義のまま最後まで再生し、以降のAttachから新定義になる
	if (reloadRequested_.exchange(false, std::memory_order_acq_rel)) { LoadData(dataPath_); }

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
		if (!KdHasDrawPassFlag(active.instance.GetParams().DrawPassFlags, pass)) { continue; }

		active.instance.Draw(active.base);
	}

	if (useEffectLayer) { postProcess.EndEffectLayer(); }
}
