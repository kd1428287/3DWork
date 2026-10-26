#include "EffectDispatcher.h"
#include "Application/Components/Tags/IRenderable.h"
#include "Framework/Shader/KdShaderManager.h"	// ※実際のパスに合わせて要調整

std::vector<EffectDispatcher*> EffectDispatcher::s_instances;
std::mutex EffectDispatcher::s_instancesMutex;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：JSONからのエフェクトデータ読み込み、イベント購読
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::Init(EventBus& bus, const std::string& effectDataPath)
{
	bus_ = &bus;
	effectDataPath_ = effectDataPath;

	LoadData(effectDataPath_);

	// ベクターの領域をあらかじめ確保して再確保コストを抑える
	subscriptions_.reserve(6);

	// すべて SubscribeScoped に統一して自動ライフサイクル管理を行う
	subscriptions_.push_back(
		bus.SubscribeScoped<Events::GenericEffectSpawn>(
			[this](const Events::GenericEffectSpawn& e) { OnGenericEffectSpawn(e); }));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::EffectAttachSpawn>(
			[this](const Events::EffectAttachSpawn& e) { OnEffectAttachSpawn(e); }));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::EffectFollowSpawn>(
			[this](const Events::EffectFollowSpawn& e) { OnEffectFollowSpawn(e); }));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::EffectPositionUpdate>(
			[this](const Events::EffectPositionUpdate& e) { OnEffectPositionUpdate(e); }));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::EffectDetach>(
			[this](const Events::EffectDetach& e) { OnEffectDetach(e); }));

	subscriptions_.push_back(
		bus.SubscribeScoped<Events::EffectStop>(
			[this](const Events::EffectStop& e) { OnEffectStop(e); }));

	{
		std::lock_guard<std::mutex> lock(s_instancesMutex);
		s_instances.push_back(this);
	}

	return true;
}
bool EffectDispatcher::LoadData(const std::string& effectDataPath)
{
	EffectDataFile data;
	if (!EffectDataLoader::Load(effectDataPath, data)) { return false; }

	// 墨の質感はファイルに記載があれば全体設定として反映する(ホットリロード時も同様)
	if (data.LiquidInk) { EffectDataLoader::ApplyLiquidInk(*data.LiquidInk); }

	return LoadEffectData(data) && LoadGroupsData(data);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// effectDataPathをEffectDataLoaderで読み込み、simpleEffects_を構築する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::LoadEffectData(const EffectDataFile& data)
{
	simpleEffects_.clear();
	instancePool_.clear();	// パラメータが変わりうるので使い回しは破棄

	for (auto& def : data.Effects)
	{
		if (def.Name.empty()) { continue; }

		EffectInstance instance;
		if (!instance.Init(def.Params, &textureProvider_)) { continue; }

		simpleEffects_[def.Name] = std::move(instance);
	}

	return true;
}

bool EffectDispatcher::LoadGroupsData(const EffectDataFile& data)
{
	groups_.clear();

	for (const auto& [key, members] : data.Groups)
	{
		std::vector<std::string> validMembers;
		validMembers.reserve(members.size());

		for (const auto& member : members) // const auto& に修正
		{
			if (simpleEffects_.find(member) != simpleEffects_.end())
			{
				validMembers.push_back(member);
			}
			else
			{
				// TODO:エラーログ
			}
		}

		groups_.emplace(key, std::move(validMembers));
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
	groups_.clear();
	activeInstances_.clear();
	instancePool_.clear();

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
		LoadData(effectDataPath_);
	}

	ProcessPendingEvents();

	for (auto& pair : simpleEffects_)
	{
		pair.second.Update(deltaTime);
	}

	for (auto it = activeInstances_.begin(); it != activeInstances_.end(); )
	{
		ActiveInstance& active = it->second;
		for (EffectInstance& instance : active.instances) { instance.Update(deltaTime, active.position); }

		// 最大再生時間を過ぎたら、新規発生を止めて残りの粒子が尽きるのを待つ
		if (active.maxDurationRemaining >= 0.0f)
		{
			active.maxDurationRemaining -= deltaTime;
			if (active.maxDurationRemaining <= 0.0f)
			{
				active.maxDurationRemaining = -1.0f;
				GracefulStop(active);
			}
		}

		// 寿命が尽きたら破棄して、インスタンスは使い回しへ返す
		if (active.lifeRemaining >= 0.0f)
		{
			active.lifeRemaining -= deltaTime;
			if (active.lifeRemaining <= 0.0f)
			{
				RecycleActive(active);
				it = activeInstances_.erase(it);
				continue;
			}
		}
		++it;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// nameに対応するインスタンスをactiveへ作る(エフェクト名なら1個、グループ名ならメンバー全て)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::CreateActiveInstances(const std::string& name, ActiveInstance& active)
{
	auto addFrom = [&](const std::string& effectName)
		{
			auto it = simpleEffects_.find(effectName);
			if (it == simpleEffects_.end()) { return; }

			// 使い回しプールにあればそれを使い、無ければ新規に作る(GPUバッファの確保を避ける)
			auto& pool = instancePool_[effectName];
			if (!pool.empty())
			{
				active.instances.push_back(std::move(pool.back()));
				pool.pop_back();
			}
			else
			{
				active.instances.emplace_back();
				if (!active.instances.back().Init(it->second.GetParams(), &textureProvider_))
				{
					active.instances.pop_back();
					return;
				}
			}
			active.effectNames.push_back(effectName);
		};

	if (simpleEffects_.find(name) != simpleEffects_.end())
	{
		addFrom(name);
	}
	else
	{
		auto git = groups_.find(name);
		if (git != groups_.end())
		{
			for (const auto& memberName : git->second) { addFrom(memberName); }
		}
	}

	return !active.instances.empty();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// activeのインスタンスを後始末して、使い回しプールへ返す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::RecycleActive(ActiveInstance& active)
{
	for (size_t i = 0; i < active.instances.size() && i < active.effectNames.size(); ++i)
	{
		auto& pool = instancePool_[active.effectNames[i]];
		if (pool.size() >= kMaxPooledPerEffect) { continue; }	// 上限を超える分は破棄

		active.instances[i].Recycle();
		pool.push_back(std::move(active.instances[i]));
	}
	active.instances.clear();
	active.effectNames.clear();
}

void EffectDispatcher::StoreActive(const std::string& key, ActiveInstance&& active)
{
	auto it = activeInstances_.find(key);
	if (it != activeInstances_.end()) { RecycleActive(it->second); }

	activeInstances_[key] = std::move(active);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 新規発生を止め、残りの粒子が尽きたら自動で破棄されるようにする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::GracefulStop(ActiveInstance& active)
{
	float remain = 0.0f;
	for (EffectInstance& instance : active.instances)
	{
		instance.StopEmission();
		remain = std::max(remain, instance.GetRemainingLife());
	}
	active.lifeRemaining = remain;
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
					auto it = simpleEffects_.find(e.Id);
					if (it != simpleEffects_.end())
					{
						it->second.Emit(e.Position, e.BaseDir);
						return;
					}

					auto git = groups_.find(e.Id);
					if (git != groups_.end())
					{
						for (const auto& memberName : git->second)
						{
							auto mit = simpleEffects_.find(memberName);
							if (mit != simpleEffects_.end())
							{
								mit->second.Emit(e.Position, e.BaseDir);
							}
						}
					}
				}
				else if constexpr (std::is_same_v<T, PendingAttach>)
				{
					ActiveInstance active;
					if (!CreateActiveInstances(e.EffectName, active)) { return; }

					active.position = e.Position;
					for (EffectInstance& instance : active.instances) { instance.Play(active.position); }

					// 呼び出し側の指定が無ければ、エフェクト定義のMaxDurationを使う
					float maxDuration = e.MaxDuration;
					if (maxDuration <= 0.0f)
					{
						for (const EffectInstance& instance : active.instances) { maxDuration = std::max(maxDuration, instance.GetParams().MaxDuration); }
					}
					if (maxDuration > 0.0f) { active.maxDurationRemaining = maxDuration; }

					StoreActive(e.InstanceKey, std::move(active));
				}
				else if constexpr (std::is_same_v<T, PendingFollowSpawn>)
				{
					ActiveInstance active;
					if (!CreateActiveInstances(e.EffectName, active)) { return; }

					active.position = e.Position;
					active.followParticles = true;

					// 最長寿命+余白が過ぎたら自動破棄
					constexpr float kLifeMargin = 0.1f;
					float maxLife = 0.0f;
					for (EffectInstance& instance : active.instances)
					{
						maxLife = std::max(maxLife, instance.EmitOnce(active.position, DirectX::SimpleMath::Vector3::Zero, e.ConvergeDuration, e.SpawnSpread));
					}
					active.lifeRemaining = maxLife + kLifeMargin;

					StoreActive(e.InstanceKey, std::move(active));
				}
				else if constexpr (std::is_same_v<T, PendingPositionUpdate>)
				{
					auto it = activeInstances_.find(e.InstanceKey);
					if (it == activeInstances_.end()) { return; }

					// 追従型は、位置の差分を粒子ごと平行移動する
					if (it->second.followParticles)
					{
						const DirectX::SimpleMath::Vector3 delta = e.Position - it->second.position;
						for (EffectInstance& instance : it->second.instances) { instance.AddFollowDelta(delta); }
					}
					it->second.position = e.Position;
				}
				else if constexpr (std::is_same_v<T, PendingDetach>)
				{
					auto it = activeInstances_.find(e.InstanceKey);
					if (it == activeInstances_.end()) { return; }

					RecycleActive(it->second);
					activeInstances_.erase(it);
				}
				else if constexpr (std::is_same_v<T, PendingStop>)
				{
					auto it = activeInstances_.find(e.InstanceKey);
					if (it == activeInstances_.end()) { return; }

					GracefulStop(it->second);
				}
			}, ev);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：保持している全エフェクトのうち、passと一致するものだけを描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Draw(ParticleDrawPass pass)
{
	// Defaultパスはエフェクト層へ描く(カラーグレードの対象外)。Brightパスは従来どおり光源用RTへ
	KdPostProcessShader& postProcess = KdShaderManager::Instance().m_postProcessShader;
	const bool useEffectLayer = (pass == ParticleDrawPass::Default);
	if (useEffectLayer) { postProcess.BeginEffectLayer(); }

	for (auto& pair : simpleEffects_)
	{
		pair.second.Draw(pass);
	}

	for (auto& pair : activeInstances_)
	{
		for (EffectInstance& instance : pair.second.instances) { instance.Draw(pass); }
	}

	if (useEffectLayer) { postProcess.EndEffectLayer(); }
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 液体描画：液体エフェクトを1つの密度RTへまとめて描き、ブラー+しきい値合成を1回だけ行う
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::DrawLiquid()
{
	bool hasLiquid = false;
	for (auto& pair : simpleEffects_) { hasLiquid |= (pair.second.IsLiquid() && pair.second.IsActive()); }
	for (auto& pair : activeInstances_) { for (EffectInstance& instance : pair.second.instances) { hasLiquid |= (instance.IsLiquid() && instance.IsActive()); } }
	if (!hasLiquid) { return; }

	KdPostProcessShader& postProcess = KdShaderManager::Instance().m_postProcessShader;

	// 液体もエフェクト層へ合成する(カラーグレードの対象外)
	postProcess.BeginEffectLayer();
	postProcess.BeginLiquid();

	for (auto& pair : simpleEffects_) { pair.second.DrawLiquid(); }
	for (auto& pair : activeInstances_) { for (EffectInstance& instance : pair.second.instances) { instance.DrawLiquid(); } }

	postProcess.EndLiquid(LiquidStyle::Ink);
	postProcess.EndEffectLayer();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 以下、イベントハンドラ：pendingEvents_へ積むだけ。simpleEffects_/activeInstances_には触れない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnGenericEffectSpawn(const Events::GenericEffectSpawn& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingSpawn{ e.Id, e.Position, e.Direction });
}

void EffectDispatcher::OnEffectAttachSpawn(const Events::EffectAttachSpawn& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingAttach{ e.InstanceKey, e.EffectName, e.Position, e.MaxDuration });
}

void EffectDispatcher::OnEffectFollowSpawn(const Events::EffectFollowSpawn& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingFollowSpawn{ e.InstanceKey, e.EffectName, e.Position, e.ConvergeDuration, e.SpawnSpread });
}

void EffectDispatcher::OnEffectPositionUpdate(const Events::EffectPositionUpdate& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingPositionUpdate{ e.InstanceKey, e.Position });
}

void EffectDispatcher::OnEffectDetach(const Events::EffectDetach& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingDetach{ e.InstanceKey });
}

void EffectDispatcher::OnEffectStop(const Events::EffectStop& e)
{
	std::lock_guard<std::mutex> lock(pendingMutex_);
	pendingEvents_.push_back(PendingStop{ e.InstanceKey });
}