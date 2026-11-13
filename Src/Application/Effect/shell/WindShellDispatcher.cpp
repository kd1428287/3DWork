#include "Application/main.h"
#include "WindShellDispatcher.h"

bool WindShellDispatcher::Init(EventBus& bus)
{
	if (!initialized_) { initialized_ = true; ++aliveCount_; }
	subscriptions_.reserve(4);

	subscriptions_.push_back(bus.SubscribeScoped<Events::WindShellAttach>(
		[this](const Events::WindShellAttach& e) { OnAttach(e); }));
	subscriptions_.push_back(bus.SubscribeScoped<Events::WindShellUpdate>(
		[this](const Events::WindShellUpdate& e) { OnUpdate(e); }));
	subscriptions_.push_back(bus.SubscribeScoped<Events::WindShellStop>(
		[this](const Events::WindShellStop& e) { OnStop(e); }));
	subscriptions_.push_back(bus.SubscribeScoped<Events::WindShellDetach>(
		[this](const Events::WindShellDetach& e) { OnDetach(e); }));

	return true;
}

void WindShellDispatcher::Release()
{
	// 先に印を下ろす。以後ハンドルはバスへ発行しない
	if (initialized_) { initialized_ = false; --aliveCount_; }

	subscriptions_.clear();

	presets_.clear();
	active_.clear();

	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.clear();
}

void WindShellDispatcher::RegisterPreset(const std::string& name, const WindShellPreset& preset)
{
	presets_[name] = preset;
}

// 毎フレーム更新：pendingの消化 → 全インスタンス更新 → 消え切ったものを破棄
void WindShellDispatcher::Update(float deltaTime)
{
	ProcessPendingEvents();

	for (auto it = active_.begin(); it != active_.end(); )
	{
		it->second.Update(deltaTime);

		if (it->second.IsFinished()) { it = active_.erase(it); }
		else { ++it; }
	}
}

// 描画：Defaultパスのみ(トレイルと同じ層)
void WindShellDispatcher::Draw(ParticleDrawPass pass)
{
	if (pass != ParticleDrawPass::Default) { return; }

	for (auto& pair : active_)
	{
		pair.second.Draw();
	}
}

void WindShellDispatcher::ProcessPendingEvents()
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
					auto pit = presets_.find(e.PresetName);
					if (pit == presets_.end())
					{
						std::cerr << "[WindShell] unknown preset: " << e.PresetName << std::endl;
						return;
					}

					// 模様がキーごとに変わるよう、ハッシュから0〜10のずらし量を作る
					const float seed = (float)(std::hash<std::string>{}(e.InstanceKey) % 1000) * 0.01f;

					WindShellInstance instance;
					instance.Play(pit->second, seed, e.Position, e.Axis);
					active_[e.InstanceKey] = std::move(instance);
				}
				else if constexpr (std::is_same_v<T, PendingUpdate>)
				{
					auto it = active_.find(e.InstanceKey);
					if (it == active_.end() || it->second.IsStopping()) { return; }

					it->second.SetTransform(e.Position, e.Axis, e.Progress);
				}
				else if constexpr (std::is_same_v<T, PendingStop>)
				{
					auto it = active_.find(e.InstanceKey);
					if (it != active_.end()) { it->second.Stop(); }
				}
				else if constexpr (std::is_same_v<T, PendingDetach>)
				{
					active_.erase(e.InstanceKey);
				}
			}, ev);
	}
}

void WindShellDispatcher::OnAttach(const Events::WindShellAttach& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingAttach{ e.InstanceKey, e.PresetName, e.Position, e.Axis });
}

void WindShellDispatcher::OnUpdate(const Events::WindShellUpdate& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingUpdate{ e.InstanceKey, e.Position, e.Axis, e.Progress });
}

void WindShellDispatcher::OnStop(const Events::WindShellStop& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingStop{ e.InstanceKey });
}

void WindShellDispatcher::OnDetach(const Events::WindShellDetach& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingDetach{ e.InstanceKey });
}