#include "Application/main.h"
#include "SlashTrailDispatcher.h"

// 初期化：イベント購読のみ行う(定義の登録はRegisterDefinition()を別途呼ぶ)
bool SlashTrailDispatcher::Init(EventBus& bus)
{
	bus_ = &bus;

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::SlashTrailBegin>(
			[this](const Events::SlashTrailBegin& e) { OnSlashTrailBegin(e); }));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::SlashTrailPositionUpdate>(
			[this](const Events::SlashTrailPositionUpdate& e) { OnSlashTrailPositionUpdate(e); }));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::SlashTrailEnd>(
			[this](const Events::SlashTrailEnd& e) { OnSlashTrailEnd(e); }));

	return true;
}

// 解放：購読解除、保持している定義・トレイル・未処理イベントの破棄
void SlashTrailDispatcher::Release()
{
	// ScopedSubscriberのデストラクタで自動的に購読解除される
	subscriptions_.clear();

	{
		std::lock_guard<std::mutex> lock(pendingMutex_);
		pendingEvents_.clear();
	}

	trailDefinitions_.clear();
	activeTrails_.clear();
	fadingTrails_.clear();

	bus_ = nullptr;
}

// トレイル定義の登録：同名なら上書きする
void SlashTrailDispatcher::RegisterDefinition(const std::string& name, const SlashTrailParams& params)
{
	trailDefinitions_[name] = params;
}

// イベントハンドラ：pendingEvents_へ積むだけ(mutexで保護)
void SlashTrailDispatcher::OnSlashTrailBegin(const Events::SlashTrailBegin& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingBegin{ e.InstanceKey, e.TrailName });
}

void SlashTrailDispatcher::OnSlashTrailPositionUpdate(const Events::SlashTrailPositionUpdate& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingPositionUpdate{ e.InstanceKey, e.Tip, e.Base });
}

void SlashTrailDispatcher::OnSlashTrailEnd(const Events::SlashTrailEnd& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingEnd{ e.InstanceKey });
}

// pendingを取り出し、積まれた順に処理する(処理中のPublishは次フレームへ回る)
void SlashTrailDispatcher::ProcessPendingEvents()
{
	std::vector<PendingEvent> events;
	{
		std::lock_guard<std::mutex> lock(pendingMutex_);
		events.swap(pendingEvents_);
	}

	for (const PendingEvent& ev : events)
	{
		std::visit([this](const auto& p) { Apply(p); }, ev);
	}
}

// 記録開始：定義から新しいインスタンスを生成する。TrailName未登録なら何もしない
//	同じキーが記録中なら、突然消さずに終了させてフェードアウトへ回してから新しく始める
void SlashTrailDispatcher::Apply(const PendingBegin& p)
{
	auto defIt = trailDefinitions_.find(p.TrailName);
	if (defIt == trailDefinitions_.end()) { return; }

	SlashTrailInstance instance;
	if (!instance.Init(defIt->second, &textureProvider_)) { return; }

	FinishRecording(p.InstanceKey);

	instance.BeginRecording();
	activeTrails_[p.InstanceKey] = std::move(instance);
}

// 記録中インスタンスの位置更新：キーが無い(Begin前・End後)場合は無視する
void SlashTrailDispatcher::Apply(const PendingPositionUpdate& p)
{
	auto it = activeTrails_.find(p.InstanceKey);
	if (it == activeTrails_.end()) { return; }

	it->second.UpdateTipBase(p.Tip, p.Base);
}

void SlashTrailDispatcher::Apply(const PendingEnd& p)
{
	FinishRecording(p.InstanceKey);
}

// 記録中→フェードアウト中へ移す(moveした後は必ずactiveTrails_から外す)
void SlashTrailDispatcher::FinishRecording(const std::string& key)
{
	auto it = activeTrails_.find(key);
	if (it == activeTrails_.end()) { return; }

	it->second.EndRecording();
	fadingTrails_.push_back(std::move(it->second));
	activeTrails_.erase(it);
}

// 毎フレーム更新：pending処理→フェードアウト中の更新と破棄→記録中の更新
void SlashTrailDispatcher::Update(float deltaTime)
{
	ProcessPendingEvents();

	for (auto& trail : fadingTrails_)
	{
		trail.Update(deltaTime);
	}

	std::erase_if(fadingTrails_, [](const SlashTrailInstance& trail) {
		return trail.IsFinished();
		});

	for (auto it = activeTrails_.begin(); it != activeTrails_.end(); )
	{
		it->second.Update(deltaTime);

		if (it->second.IsRecording())
		{
			++it;
			continue;
		}

		// 最大記録時間で自動終了した分は、Endを待たずフェードアウト側へ移す
		fadingTrails_.push_back(std::move(it->second));
		it = activeTrails_.erase(it);
	}
}

// 描画：DrawPassFlagsにpassが含まれるトレイルだけを描画する
void SlashTrailDispatcher::Draw(ParticleDrawPass pass)
{
	for (auto& pair : activeTrails_)
	{
		pair.second.Draw(pass);
	}

	for (auto& trail : fadingTrails_)
	{
		trail.Draw(pass);
	}
}
