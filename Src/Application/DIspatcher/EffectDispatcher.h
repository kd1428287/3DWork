#pragma once

#include "../Engine/EventBus/Event/EffectEvents.h"
#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"
#include "../Effect/EffectDataLoader.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト生成ディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、エフェクト生成イベントを受け取ったら
// 対応するKdGPUParticleへEmitを行う。
//
// ・座標だけで足りる単純なエフェクト(HitSpark等) → GenericEffectSpawnEvent
//   → EffectId→発生設定の対応表(m_simpleEffects)を引いて処理
// ・鍔迫り合いの火花(専用の具象イベント型、方向が要る) → WeaponClashEffectEvent
//   → 専用ハンドラ・専用のKdGPUParticleを個別に持つ
//
// どちらのパラメータもInit()時にEffectDataLoaderでJSON(effectDataPath)を読み込んで構築する
// (EffectEditorが保存したものと同じファイルを指定すれば、エディタで調整した
// パラメータがそのままゲーム実行時にも反映される)。
// ・m_simpleEffectsは、JSON側の"name"がEffectId名と一致する項目のみ登録される
// ・m_weaponClashParamsは、JSONの"weaponClash"項目が無い/読み込み失敗時は
//   WeaponClashEffectParams()のデフォルト値(元のハードコード値と同一)のまま
//
// 【使い方】
//   EffectDispatcher dispatcher;
//   dispatcher.Init(sceneLocalEventBus, "Asset/Data/effectmap.json");
//   :
//   // 毎フレーム
//   dispatcher.Update(deltaTime);
//   dispatcher.Draw();
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectDispatcher
{
public:

	EffectDispatcher() {}
	~EffectDispatcher() { Release(); }

	// busはシーンのライフサイクルに合わせて破棄されるバスを渡す想定
	// effectDataPathはEffectEditorが保存するJSONと同じものを指定する(既定はEditor側の初期パスと同じ)
	bool Init(EventBus& bus, const std::string& effectDataPath = "Asset/Data/Game/effectmap.json");

	void Release();

	// 生きているパーティクルの更新のみ行う(発生はイベント経由でのみ起こる)
	void Update(float deltaTime);

	// 保持している全エフェクトの描画
	void Draw();

private:

	// GenericEffectSpawnEventを受け取った時の処理
	void OnGenericEffectSpawn(const Events::Effect::GenericEffectSpawnEvent& e);

	// WeaponClashEffectEventを受け取った時の処理
	void OnWeaponClash(const Events::Effect::WeaponClashEffectEvent& e);

	// 単純エフェクト用の対応表エントリ
	struct SimpleEffectEntry
	{
		std::shared_ptr<KdGPUParticle>		Particle;
		std::shared_ptr<KdTexture>			Texture;
		KdGPUParticle::EmitParameter		ParamTemplate;	// Positionだけ上書きして使う
		UINT								EmitCount = 20;
	};

	// effectDataPathをEffectDataLoaderで読み込み、m_simpleEffectsとm_weaponClashParamsを構築する
	// (JSONが無い/読み込み失敗時はどちらもデフォルト値のまま：単純エフェクトは1つも
	//  登録されなくなるが、鍔迫り合いの火花は元のハードコード値と同じ見た目で動作する)
	bool LoadEffectData(const std::string& effectDataPath);

	// EffectId → 発生設定
	std::unordered_map<EffectId, SimpleEffectEntry> m_simpleEffects;

	// 鍔迫り合いの火花専用
	std::shared_ptr<KdGPUParticle>	m_clashSparkParticle;
	std::shared_ptr<KdTexture>		m_clashSparkTexture;
	WeaponClashEffectParams			m_weaponClashParams;

	// 購読の自動解除用(Release時にまとめて解除される)
	std::vector<ScopedSubscriber> m_subscriptions;

	EventBus* m_bus = nullptr;
};