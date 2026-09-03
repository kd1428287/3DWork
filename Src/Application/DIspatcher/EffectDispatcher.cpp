#include "EffectDispatcher.h"
#include "../Components/Tags/IRenderable.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：JSONからのエフェクトデータ読み込み、各パーティクル生成、イベント購読
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::Init(EventBus& bus, const std::string& effectDataPath)
{
	m_bus = &bus;

	// 先にJSONを読み込む(鍔迫り合い用パーティクルのMaxParticleNumがここで決まる為)
	LoadEffectData(effectDataPath);

	//------------------------------------------
	// 鍔迫り合いの火花専用パーティクル
	//------------------------------------------
	m_clashSparkParticle = std::make_shared<KdGPUParticle>();
	if (!m_clashSparkParticle->Init(m_weaponClashParams.MaxParticleNum))
	{
		assert(0 && "EffectDispatcher：火花用パーティクル初期化失敗");
		return false;
	}

	// TODO：既存のリソース管理の仕組みに合わせてテクスチャをロードしてセットする
	// 例：m_clashSparkTexture = KdResourceFactory::Instance().GetTexture("Asset/Texture/spark.png");

	//------------------------------------------
	// イベント購読
	// ※ScopedSubscriberに積んでおくことで、Release時にまとめて自動解除される
	//------------------------------------------
	m_subscriptions.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::GenericEffectSpawnEvent>(
			[this](const Events::Effect::GenericEffectSpawnEvent& e) { OnGenericEffectSpawn(e); }));

	m_subscriptions.emplace_back(
		&bus,
		bus.Subscribe<Events::Effect::WeaponClashEffectEvent>(
			[this](const Events::Effect::WeaponClashEffectEvent& e) { OnWeaponClash(e); }));

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// effectDataPathをEffectDataLoaderで読み込み、m_simpleEffectsとm_weaponClashParamsを構築する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::LoadEffectData(const std::string& effectDataPath)
{
	m_simpleEffects.clear();

	EffectDataFile data;
	if (!EffectDataLoader::Load(effectDataPath, data))
	{
		// JSONが無い/読み込み失敗：m_weaponClashParamsはデフォルト値(元のハードコード値)のまま、
		// m_simpleEffectsは空のまま(単純エフェクトは発生しなくなる)で継続する
		return false;
	}

	m_weaponClashParams = data.WeaponClash;

	for (auto& def : data.Effects)
	{
		EffectId id;
		if (!EffectDataLoader::NameToEffectId(def.Name, id))
		{
			// EffectIdに対応しない名前(タイポ等)は無視する
			continue;
		}

		SimpleEffectEntry entry;

		entry.Particle = std::make_shared<KdGPUParticle>();
		if (!entry.Particle->Init(def.Params.MaxParticleNum))
		{
			assert(0 && "EffectDispatcher：JSONから読み込んだエフェクト用パーティクル初期化失敗");
			continue;
		}

		// TODO：既存のリソース管理の仕組みに合わせてdef.Params.TexturePathからテクスチャをロードしてセットする
		// 例：entry.Texture = KdResourceFactory::Instance().GetTexture("Asset/Texture/" + def.Params.TexturePath);

		//entry.ParamTemplate = def.Params.ToEmitParameter({ 0.0f, 0.0f, 0.0f });	// Positionはイベント発生時に上書きする
		//entry.EmitCount = (UINT)std::max(0, def.Params.EmitCount);

		m_simpleEffects[id] = entry;
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放：購読解除、保持しているパーティクル・テクスチャの破棄
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Release()
{
	// ScopedSubscriberのデストラクタで自動的に購読解除される
	m_subscriptions.clear();

	m_simpleEffects.clear();

	m_clashSparkParticle.reset();
	m_clashSparkTexture.reset();

	m_bus = nullptr;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 毎フレーム更新：保持している全パーティクルのシミュレーションを進める
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Update(float deltaTime)
{
	if (m_clashSparkParticle)
	{
		m_clashSparkParticle->Update(deltaTime);
	}

	for (auto& pair : m_simpleEffects)
	{
		SimpleEffectEntry& entry = pair.second;

		if (entry.Particle)
		{
			entry.Particle->Update(deltaTime);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：保持している全パーティクルを描画する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Draw()
{
	if (m_clashSparkParticle)
	{
		m_clashSparkParticle->Draw(m_clashSparkTexture);
	}

	for (auto& pair : m_simpleEffects)
	{
		SimpleEffectEntry& entry = pair.second;

		if (entry.Particle)
		{
			entry.Particle->Draw(entry.Texture);
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 単純エフェクト発生イベントの処理：対応表を引いてEmitするだけ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnGenericEffectSpawn(const Events::Effect::GenericEffectSpawnEvent& e)
{
	auto it = m_simpleEffects.find(e.Id);

	// 対応表に無いEffectIdは無視(JSON未定義、または呼び出し側のミス)
	if (it == m_simpleEffects.end()) { return; }

	SimpleEffectEntry& entry = it->second;

	KdGPUParticle::EmitParameter param = entry.ParamTemplate;
	param.Position = e.Position;

	entry.Particle->Emit(param, entry.EmitCount);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 鍔迫り合いの火花発生イベントの処理
//	発生方向(baseDir)の計算のみここで行い、Emitパラメータ自体は
//	m_weaponClashParams(JSONから読み込んだ、またはデフォルトの)Main/Emberに委譲する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::OnWeaponClash(const Events::Effect::WeaponClashEffectEvent& e)
{
	if (!m_clashSparkParticle) { return; }

	// 両武器の進行方向の差分を、火花が飛び散る基準方向として採用する簡易実装
	// (正確な反射方向の計算はせず、それっぽく見える近似で済ませている)
	Math::Vector3 baseDir = e.SelfWeaponDir - e.OtherWeaponDir;

	if (baseDir.LengthSquared() < 0.0001f)
	{
		// 方向が定まらない(ほぼ同じ向き)場合は上向きにフォールバック
		baseDir = Math::Vector3(0.0f, 1.0f, 0.0f);
	}

	baseDir.Normalize();

	// パリィ成功時は派手に、通常ガードのブロックは控えめにする(Count自体もJSONで調整可能)
	m_clashSparkParticle->Emit(
		m_weaponClashParams.Main.ToEmitParameter(e.Position, baseDir),
		m_weaponClashParams.Main.GetEmitCount(e.IsParry));

	m_clashSparkParticle->Emit(
		m_weaponClashParams.Ember.ToEmitParameter(e.Position, baseDir),
		m_weaponClashParams.Ember.GetEmitCount(e.IsParry));
}