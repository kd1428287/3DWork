#include "../main.h"
#include "SlashTrailDispatcher.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：イベント購読のみ行う(定義の登録はRegisterDefinition()を別途呼んでもらう)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool SlashTrailDispatcher::Init(EventBus& bus)
{
	bus_ = &bus;

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::SlashTrail::SlashTrailBeginEvent>(
			[this](const Events::SlashTrail::SlashTrailBeginEvent& e) { OnSlashTrailBegin(e); }));

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::SlashTrail::SlashTrailPositionUpdateEvent>(
			[this](const Events::SlashTrail::SlashTrailPositionUpdateEvent& e) { OnSlashTrailPositionUpdate(e); }));

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::SlashTrail::SlashTrailEndEvent>(
			[this](const Events::SlashTrail::SlashTrailEndEvent& e) { OnSlashTrailEnd(e); }));

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放：購読解除、保持しているトレイル定義・アクティブトレイルの破棄
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailDispatcher::Release()
{
	// ScopedSubscriberのデストラクタで自動的に購読解除される
	subscriptions_.clear();

	trailDefinitions_.clear();
	activeTrails_.clear();

	bus_ = nullptr;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル定義の登録：同名なら上書きする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailDispatcher::RegisterDefinition(const std::string& name, const SlashTrailParams& params)
{
	trailDefinitions_[name] = params;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新：全アクティブトレイルを更新し、フェードアウトし切ったものを刈り取る
//	※EndRecording()直後ではなく、IsFinished()==trueになって初めて破棄する点に注意
//	  (EffectDispatcherのDetach即破棄とは違う挙動)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailDispatcher::Update(float deltaTime)
{
	for (auto it = activeTrails_.begin(); it != activeTrails_.end(); )
	{
		it->second.Update(deltaTime);

		if (it->second.IsFinished())
		{
			it = activeTrails_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：DrawPassFlagsにpassが含まれるアクティブトレイルだけを描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailDispatcher::Draw(ParticleDrawPass pass)
{
	for (auto& pair : activeTrails_)
	{
		pair.second.Draw(pass);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録開始：TrailNameの定義からInstanceKey専用の新しいSlashTrailInstanceを生成し、記録を始める
//	・TrailNameが未登録の場合は何もしない(呼び出し側のミス、またはRegisterDefinition忘れ)
//	・同じInstanceKeyで既にアクティブなものがある場合は、フェードアウトを待たずに
//	  上書きする(通常は前の記録がIsFinished()になってから次のBeginが来る想定だが、
//	  連撃等で前の記録が残ったまま次のBeginが来た場合は新しい記録を優先する)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailDispatcher::OnSlashTrailBegin(const Events::SlashTrail::SlashTrailBeginEvent& e)
{
	auto defIt = trailDefinitions_.find(e.TrailName);
	if (defIt == trailDefinitions_.end()) { return; }

	SlashTrailInstance instance;
	if (!instance.Init(defIt->second)) { return; }

	instance.BeginRecording();

	activeTrails_[e.InstanceKey] = std::move(instance);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録中インスタンスの位置更新：InstanceKeyが見つからない場合(Begin前・End後)は無視する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailDispatcher::OnSlashTrailPositionUpdate(const Events::SlashTrail::SlashTrailPositionUpdateEvent& e)
{
	auto it = activeTrails_.find(e.InstanceKey);
	if (it == activeTrails_.end()) { return; }

	it->second.UpdateTipBase(e.Tip, e.Base);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録終了：EndRecording()を呼ぶだけ(即座には破棄しない。Update()の刈り取りに委ねる)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void SlashTrailDispatcher::OnSlashTrailEnd(const Events::SlashTrail::SlashTrailEndEvent& e)
{
	auto it = activeTrails_.find(e.InstanceKey);
	if (it == activeTrails_.end()) { return; }

	it->second.EndRecording();
}
