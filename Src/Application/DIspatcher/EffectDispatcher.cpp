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

	m_clashSparkTexture = KdAssets::Instance().m_textures.GetData("Asset/Textures/Game/Effect/effect.png");

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
//	単純エフェクトの生成・テクスチャ解決はEffectInstance::Init()に委譲する
//	(EffectEditor側のプレビュー初期化と全く同じ経路を通る為、ここでGravity/テクスチャの
//	 適用漏れが起きる余地は無い)
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

		EffectInstance instance;
		if (!instance.Init(def.Params, &m_textureProvider))
		{
			// 失敗理由(KdGPUParticle初期化失敗)はEffectInstance::Init内でassert済み
			continue;
		}

		m_simpleEffects[id] = std::move(instance);
	}

	return true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放：購読解除、保持しているエフェクト・パーティクル・テクスチャの破棄
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
// 毎フレーム更新：保持している全エフェクトのシミュレーションを進める
//	Gravityの適用はEffectInstance::Update()内で行われる。
//	m_simpleEffectsはPlay()を使わずOnGenericEffectSpawn()の都度Emit()するだけの一発仕様の為、
//	worldPos/baseDirを省略して呼んでよい(内部のm_isPlayingがfalseのままなので自動発生処理はスキップされ、
//	Gravity適用のみが行われる)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Update(float deltaTime)
{
	if (m_clashSparkParticle)
	{
		m_clashSparkParticle->Update(deltaTime);
	}

	for (auto& pair : m_simpleEffects)
	{
		pair.second.Update(deltaTime);
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画：保持している全エフェクトを描画する
//	テクスチャの解決はEffectInstance::Init/Reconfigure時に済んでいる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void EffectDispatcher::Draw()
{
	if (m_clashSparkParticle)
	{
		m_clashSparkParticle->Draw(m_clashSparkTexture);
	}

	for (auto& pair : m_simpleEffects)
	{
		pair.second.Draw();
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

	// 単発発生：定義されている全Layersぶんをまとめて発生させる(baseDirは未使用の為省略)
	it->second.Emit(e.Position);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 鍔迫り合いの火花発生イベントの処理
//	発生方向(baseDir)の計算のみここで行い、Emitパラメータ自体は
//	m_weaponClashParams(JSONから読み込んだ、またはデフォルトの)Main/Emberに委譲する
//	※EffectInstanceの対象範囲外。理由はEffectDispatcher.hのクラスコメント参照
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