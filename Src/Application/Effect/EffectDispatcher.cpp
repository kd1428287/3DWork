#include "EffectDispatcher.h"
#include "../Components/Tags/IRenderable.h"

// 生存中の全EffectDispatcherインスタンス(NotifyDataSaved()用)
std::vector<EffectDispatcher*> EffectDispatcher::s_instances;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：JSONからのエフェクトデータ読み込み、イベント購読
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::Init(EventBus& bus, const std::string& effectDataPath)
{
	bus_ = &bus;
	effectDataPath_ = effectDataPath;

	LoadEffectData(effectDataPath_);

	//------------------------------------------
	// イベント購読
	// ※ScopedSubscriberに積んでおくことで、Release時にまとめて自動解除される
	//------------------------------------------
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

	// NotifyDataSaved()から辿れるように自身を登録しておく
	s_instances.push_back(this);

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// effectDataPathをEffectDataLoaderで読み込み、simpleEffects_を構築する
//	単純エフェクトも鍔迫り合いの火花(WeaponClashParry/Block)も、区別なく同じループで
//	EffectInstance::Init()に渡される(テクスチャ解決も含め、どちらも通常のGPUParticleParams/
//	Layersでしかない為。以前ここにあった"Asset/Textures/Game/Effect/effect.png"の直接ロードは
//	不要になった：鍔迫り合いのテクスチャもJSON側のtextureフィールドで指定する)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放：購読解除、保持しているエフェクトの破棄
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Release()
{
	// s_instancesから自身を除去(NotifyDataSaved()が破棄済みインスタンスを辿らないように)
	s_instances.erase(std::remove(s_instances.begin(), s_instances.end(), this), s_instances.end());

	// ScopedSubscriberのデストラクタで自動的に購読解除される
	subscriptions_.clear();

	simpleEffects_.clear();
	activeInstances_.clear();

	bus_ = nullptr;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// pathと同じJSONを読み込んでいる、生存中の全EffectDispatcherインスタンスへ再ロードを促す
//	EffectEditor::Save()が保存成功時に呼ぶ想定。該当インスタンスが無ければ何もしない
//	(実行中でない、あるいは別のJSONを見ているだけなので、これは正常なケース)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::NotifyDataSaved(const std::string& path)
{
	for (EffectDispatcher* dispatcher : s_instances)
	{
		if (dispatcher->effectDataPath_ == path)
		{
			dispatcher->LoadEffectData(path);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新：保持している全エフェクトのシミュレーションを進める
//	Gravityの適用はEffectInstance::Update()内で行われる
//	・simpleEffects_  ：単発発生のみなので位置は使わない(deltaTimeだけ渡せば十分)
//	・activeInstances_：Attach中の発生源に追従させる必要があるので、
//	                    直近のEffectPositionUpdateEventで受け取ったpositionを毎回渡す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
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
// 単純エフェクト発生イベントの処理：対応表を引いてEmitするだけ(baseDirは使わない)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnGenericEffectSpawn(const Events::Effect::GenericEffectSpawnEvent& e)
{
	auto it = simpleEffects_.find(e.Id);

	// 対応表に無いstd::stringは無視(JSON未定義、または呼び出し側のミス)
	if (it == simpleEffects_.end()) { return; }

	it->second.Emit(e.Position);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 鍔迫り合いの火花発生イベントの処理
//	パリィ/ブロックで別々のstd::string(WeaponClashParry/WeaponClashBlock)を引く。
//	どちらもJSON上は"Main"/"Ember"に相当する2つのLayersを持つ通常のGPUParticleParamsであり、
//	OnGenericEffectSpawn()と同じsimpleEffects_から取り出す。
//	このイベント固有の処理として残るのは、baseDir(発生方向)の計算だけ。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnWeaponClash(const Events::Effect::WeaponClashEffectEvent& e)
{
	const std::string id = e.IsParry ? "WeaponClashParry" : "WeaponClashBlock";

	auto it = simpleEffects_.find(id);

	// JSON未定義の場合は何も発生しない(OnGenericEffectSpawn()と同じ扱い)
	if (it == simpleEffects_.end()) { return; }

	// 両武器の進行方向の差分を、火花が飛び散る基準方向として採用する簡易実装
	// (正確な反射方向の計算はせず、それっぽく見える近似で済ませている)
	Math::Vector3 baseDir = e.SelfWeaponDir - e.OtherWeaponDir;

	if (baseDir.LengthSquared() < 0.0001f)
	{
		// 方向が定まらない(ほぼ同じ向き)場合は上向きにフォールバック
		baseDir = Math::Vector3(0.0f, 1.0f, 0.0f);
	}

	baseDir.Normalize();

	it->second.Emit(e.Position, baseDir);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 継続再生の開始：EffectNameのテンプレート(simpleEffects_)からパラメータを借用し、
// InstanceKey専用の新しいEffectInstanceをactiveInstances_に生成してPlay()する
//	・EffectNameがJSON未定義の場合は何もしない(simpleEffects_に無い名前を指定した呼び出し側のミス)
//	・同じInstanceKeyで再度Attachされた場合は、既存のものを止めずに上書きする
//	  (呼び出し側が同じキーを使い回すのは想定外の呼び方なので、警告を出したい場合はここに追加すること)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 継続再生中インスタンスの位置更新：発生源が動く場合、呼び出し元が毎フレーム発行する想定
//	InstanceKeyが見つからない場合(Attach前、またはDetach済み)は無視する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnEffectPositionUpdate(const Events::Effect::EffectPositionUpdateEvent& e)
{
	auto it = activeInstances_.find(e.InstanceKey);
	if (it == activeInstances_.end()) { return; }

	it->second.position = e.Position;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 継続再生の終了：activeInstances_から当該インスタンスを破棄する
//	※既存の生存パーティクルも含めて即座に消える(フェードアウトはしない。EffectDispatcher.hのコメント参照)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnEffectDetach(const Events::Effect::EffectDetachEvent& e)
{
	activeInstances_.erase(e.InstanceKey);
}