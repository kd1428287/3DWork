#include "EffectDispatcher.h"
#include "Application/Components/Tags/IRenderable.h"

std::vector<EffectDispatcher*> EffectDispatcher::s_instances;
std::mutex EffectDispatcher::s_instancesMutex;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：JSONからのエフェクトデータ読み込み、イベント購読
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::Init(EventBus& bus, const std::string& effectDataPath)
{
	bus_ = &bus;
	effectDataPath_ = effectDataPath;

	LoadEffectData(effectDataPath_);

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::GenericEffectSpawnEvent>(
			[this](const Events::Effect::GenericEffectSpawnEvent& e) { OnGenericEffectSpawn(e); }));

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::EffectAttachSpawnEvent>(
			[this](const Events::Effect::EffectAttachSpawnEvent& e) { OnEffectAttachSpawn(e); }));

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::EffectPositionUpdateEvent>(
			[this](const Events::Effect::EffectPositionUpdateEvent& e) { OnEffectPositionUpdate(e); }));

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::EffectDetachEvent>(
			[this](const Events::Effect::EffectDetachEvent& e) { OnEffectDetach(e); }));

	{
		std::lock_guard<std::mutex> lock(s_instancesMutex);
		s_instances.push_back(this);
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// effectDataPathをEffectDataLoaderで読み込み、simpleEffects_を構築する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::LoadEffectData(const std::string& effectDataPath)
{
	simpleEffects_.clear();

	EffectDataFile data;
	if (!EffectDataLoader::Load(effectDataPath, data)) { return false; }

	for (auto& def : data.Effects)
	{
		if (def.Name.empty()) { continue; }

		EffectInstance instance;
		if (!instance.Init(def.Params, &textureProvider_)) { continue; }

		simpleEffects_[def.Name] = std::move(instance);
	}
	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放：購読解除、保持しているエフェクトの破棄
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Release()
{
	{
		std::lock_guard<std::mutex> lock(s_instancesMutex);
		s_instances.erase(std::remove(s_instances.begin(), s_instances.end(), this), s_instances.end());
	}

	subscriptions_.clear();

	simpleEffects_.clear();
	activeInstances_.clear();

	{
		std::lock_guard<std::mutex> lock(pendingMutex_);
		pendingEvents_.clear();
	}

	bus_ = nullptr;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// pathと同じJSONを読み込んでいる、生存中の全EffectDispatcherへ再ロード要求を立てる
// (実際のLoadEffectData()は各インスタンスの次回Update()冒頭で行われる)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::NotifyDataSaved(const std::string& path)
{
	std::lock_guard<std::mutex> lock(s_instancesMutex);

	for (EffectDispatcher* dispatcher : s_instances)
	{
		if (dispatcher->effectDataPath_ == path)
		{
			dispatcher->reloadRequested_.store(true, std::memory_order_release);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新：再ロード要求 → pendingEvents_の消化 → 全エフェクトのシミュレーション更新
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Update(float deltaTime)
{
	if (reloadRequested_.exchange(false, std::memory_order_acq_rel))
	{
		LoadEffectData(effectDataPath_);
	}

	ProcessPendingEvents();

	for (auto& pair : simpleEffects_)
	{
		pair.second.Update(deltaTime);
	}

	for (auto& pair : activeInstances_)
	{
		ActiveInstance& active = pair.second;
		active.instance.Update(deltaTime, active.position);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// pendingEvents_を取り出して1件ずつ処理する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::ProcessPendingEvents()
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

				if constexpr (std::is_same_v<T, PendingSpawn>)
				{
					// 対応表に無いIdは無視(JSON未定義、または呼び出し側のミス)
					//	鍔迫り合いの火花("WeaponClashParry"/"WeaponClashBlock")も
					//	通常のIdの1つとしてここで処理される
					auto it = simpleEffects_.find(e.Id);
					if (it == simpleEffects_.end()) { return; }
					it->second.Emit(e.Position, e.BaseDir);
				}
				else if constexpr (std::is_same_v<T, PendingAttach>)
				{
					auto templateIt = simpleEffects_.find(e.EffectName);
					if (templateIt == simpleEffects_.end()) { return; }

					ActiveInstance active;
					if (!active.instance.Init(templateIt->second.GetParams(), &textureProvider_)) { return; }

					active.position = e.Position;
					active.instance.Play(active.position);

					activeInstances_[e.InstanceKey] = std::move(active);
				}
				else if constexpr (std::is_same_v<T, PendingPositionUpdate>)
				{
					auto it = activeInstances_.find(e.InstanceKey);
					if (it == activeInstances_.end()) { return; }
					it->second.position = e.Position;
				}
				else if constexpr (std::is_same_v<T, PendingDetach>)
				{
					activeInstances_.erase(e.InstanceKey);
				}
			}, ev);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：保持している全エフェクトのうち、passと一致するものだけを描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Draw(ParticleDrawPass pass)
{
	for (auto& pair : simpleEffects_)
	{
		pair.second.Draw(pass);
	}

	for (auto& pair : activeInstances_)
	{
		pair.second.instance.Draw(pass);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 以下、イベントハンドラ：pendingEvents_へ積むだけ。simpleEffects_/activeInstances_には触れない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnGenericEffectSpawn(const Events::Effect::GenericEffectSpawnEvent& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingSpawn{ e.Id, e.Position, e.Direction });
}

void EffectDispatcher::OnEffectAttachSpawn(const Events::Effect::EffectAttachSpawnEvent& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingAttach{ e.InstanceKey, e.EffectName, e.Position });
}

void EffectDispatcher::OnEffectPositionUpdate(const Events::Effect::EffectPositionUpdateEvent& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingPositionUpdate{ e.InstanceKey, e.Position });
}

void EffectDispatcher::OnEffectDetach(const Events::Effect::EffectDetachEvent& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingDetach{ e.InstanceKey });
}