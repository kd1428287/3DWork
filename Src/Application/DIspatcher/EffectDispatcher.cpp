#include "EffectDispatcher.h"
#include "../Components/Tags/IRenderable.h"

// 初期化：JSONからのエフェクトデータ読み込み、イベント購読
bool EffectDispatcher::Init(EventBus& bus, const std::string& effectDataPath)
{
	bus_ = &bus;

	LoadEffectData(effectDataPath);

	// イベント購読
	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::GenericEffectSpawnEvent>(
			[this](const Events::Effect::GenericEffectSpawnEvent& e) { OnGenericEffectSpawn(e); }));

	subscriptions_.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::WeaponClashEffectEvent>(
			[this](const Events::Effect::WeaponClashEffectEvent& e) { OnWeaponClash(e); }));

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

	return true;
}

bool EffectDispatcher::LoadEffectData(const std::string& effectDataPath)
{
	simpleEffects_.clear();

	EffectDataFile data;
	if (!EffectDataLoader::Load(effectDataPath, data)) { return false; }

	for (auto& def : data.Effects)
	{
		if (def.Name.empty()) { continue; } // 名前無しだけは弾く

		EffectInstance instance;
		if (!instance.Init(def.Params, &textureProvider_)) { continue; }

		simpleEffects_[def.Name] = std::move(instance); // ← 変換不要、名前=キー
	}
	return true;
}

void EffectDispatcher::Release()
{
	// ScopedSubscriberのデストラクタで自動的に購読解除される
	subscriptions_.clear();

	simpleEffects_.clear();
	activeInstances_.clear();

	bus_ = nullptr;
}

void EffectDispatcher::Update(float deltaTime)
{
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

void EffectDispatcher::Draw()
{
	for (auto& pair : simpleEffects_)
	{
		pair.second.Draw();
	}

	for (auto& pair : activeInstances_)
	{
		pair.second.instance.Draw();
	}
}

void EffectDispatcher::OnGenericEffectSpawn(const Events::Effect::GenericEffectSpawnEvent& e)
{
	auto it = simpleEffects_.find(e.Id);

	// 対応表に無いstd::stringは無視(JSON未定義、または呼び出し側のミス)
	if (it == simpleEffects_.end()) { return; }

	it->second.Emit(e.Position);
}

void EffectDispatcher::OnWeaponClash(const Events::Effect::WeaponClashEffectEvent& e)
{
	const std::string id = e.IsParry ? "WeaponClashParry" : "WeaponClashBlock";

	auto it = simpleEffects_.find(id);

	// JSON未定義の場合は何も発生しない(OnGenericEffectSpawn()と同じ扱い)
	if (it == simpleEffects_.end()) { return; }

	// 両武器の進行方向の差分を、火花が飛び散る基準方向として採用する簡易実装
	Math::Vector3 baseDir = e.SelfWeaponDir - e.OtherWeaponDir;

	if (baseDir.LengthSquared() < 0.0001f)
	{
		// 方向が定まらない(ほぼ同じ向き)場合は上向きにフォールバック
		baseDir = Math::Vector3(0.0f, 1.0f, 0.0f);
	}

	baseDir.Normalize();

	it->second.Emit(e.Position, baseDir);
}

void EffectDispatcher::OnEffectAttachSpawn(const Events::Effect::EffectAttachSpawnEvent& e)
{
	auto templateIt = simpleEffects_.find(e.EffectName);
	if (templateIt == simpleEffects_.end()) { return; }

	ActiveInstance active;
	if (!active.instance.Init(templateIt->second.GetParams(), &textureProvider_)) { return; }

	active.position = e.Position;
	active.instance.Play(active.position);

	activeInstances_[e.InstanceKey] = std::move(active);
}

void EffectDispatcher::OnEffectPositionUpdate(const Events::Effect::EffectPositionUpdateEvent& e)
{
	auto it = activeInstances_.find(e.InstanceKey);
	if (it == activeInstances_.end()) { return; }

	it->second.position = e.Position;
}

void EffectDispatcher::OnEffectDetach(const Events::Effect::EffectDetachEvent& e)
{
	activeInstances_.erase(e.InstanceKey);
}