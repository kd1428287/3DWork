#include "EffectDispatcher.h"
#include "../Components/Tags/IRenderable.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化：各エフェクト用のKdGPUParticle生成、対応表登録、イベント購読
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool EffectDispatcher::Init(EventBus& bus)
{
	m_bus = &bus;

	//------------------------------------------
	// 鍔迫り合いの火花専用パーティクル
	//------------------------------------------
	m_clashSparkParticle = std::make_shared<KdGPUParticle>();
	if (!m_clashSparkParticle->Init(2000))
	{
		assert(0 && "EffectDispatcher：火花用パーティクル初期化失敗");
		return false;
	}

	// TODO：既存のリソース管理の仕組みに合わせてテクスチャをロードしてセットする
	// 例：m_clashSparkTexture = KdResourceFactory::Instance().GetTexture("Asset/Texture/spark.png");

	//------------------------------------------
	// 単純エフェクト(座標だけで足りるもの)の対応表
	// ※ここでは例としてFootDustのみ登録。HitSpark/BloodSplatterも
	//   必要になったタイミングで同様にエントリを追加する
	//------------------------------------------
	{
		SimpleEffectEntry entry;

		entry.Particle = std::make_shared<KdGPUParticle>();
		if (!entry.Particle->Init(500))
		{
			assert(0 && "EffectDispatcher：FootDust用パーティクル初期化失敗");
			return false;
		}

		// TODO：テクスチャは実際のアセットに差し替える
		// entry.Texture = KdResourceFactory::Instance().GetTexture("Asset/Texture/dust.png");

		entry.ParamTemplate.VelocityMin = { -0.3f, 0.0f, -0.3f };
		entry.ParamTemplate.VelocityMax = { 0.3f, 0.5f, 0.3f };
		entry.ParamTemplate.SizeMin = 0.05f;
		entry.ParamTemplate.SizeMax = 0.1f;
		entry.ParamTemplate.LifeMin = 0.3f;
		entry.ParamTemplate.LifeMax = 0.6f;
		entry.ParamTemplate.Color = { 0.6f, 0.5f, 0.4f, 1.0f };
		entry.EmitCount = 15;

		m_simpleEffects[EffectId::FootDust] = entry;
	}

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

	// 対応表に無いEffectIdは無視(未登録のエフェクトを指定した呼び出し側のミス)
	if (it == m_simpleEffects.end()) { return; }

	SimpleEffectEntry& entry = it->second;

	KdGPUParticle::EmitParameter param = entry.ParamTemplate;
	param.Position = e.Position;

	entry.Particle->Emit(param, entry.EmitCount);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 鍔迫り合いの火花発生イベントの処理
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

	// パリィ成功時は派手に、通常ガードのブロックは控えめにする
	const UINT mainCount = e.IsParry ? 120 : 60;
	const UINT emberCount = e.IsParry ? 75 : 50;

	//------------------------------------------
	// 勢いよく飛ぶ火花(メイン)
	//------------------------------------------
	KdGPUParticle::EmitParameter mainParam;
	mainParam.Position = e.Position;
	mainParam.VelocityMin = baseDir * 1.0f - Math::Vector3(1.0f, 0.5f, 1.0f);
	mainParam.VelocityMax = baseDir * 4.0f + Math::Vector3(1.0f, 1.5f, 1.0f);
	mainParam.SizeMin = 0.02f;
	mainParam.SizeMax = 0.06f;
	mainParam.LifeMin = 0.45f;
	mainParam.LifeMax = 0.8f;
	mainParam.Color = { 1.0f, 0.85f, 0.4f, 1.0f }; // 明るいオレンジ〜黄色

	m_clashSparkParticle->Emit(mainParam, mainCount);

	//------------------------------------------
	// ゆっくり落ちるくすぶり(厚みを出すための2回目のEmit)
	//------------------------------------------
	KdGPUParticle::EmitParameter emberParam;
	emberParam.Position = e.Position;
	emberParam.VelocityMin = baseDir * 0.3f - Math::Vector3(0.3f, 0.1f, 0.3f);
	emberParam.VelocityMax = baseDir * 1.0f + Math::Vector3(0.3f, 0.3f, 0.3f);
	emberParam.SizeMin = 0.02f;
	emberParam.SizeMax = 0.05f;
	emberParam.LifeMin = 0.3f;
	emberParam.LifeMax = 0.6f;
	emberParam.Color = { 1.0f, 0.5f, 0.1f, 1.0f }; // 暗めの赤オレンジ

	m_clashSparkParticle->Emit(emberParam, emberCount);
}