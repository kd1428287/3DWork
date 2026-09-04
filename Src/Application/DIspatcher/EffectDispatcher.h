#pragma once

#include "../Engine/EventBus/Event/EffectEvents.h"
#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"
#include "../Effect/EffectDataLoader.h"
#include "../Effect/EffectInstance.h"
#include "../Effect/KdAssetsTextureProvider.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト生成ディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、エフェクト生成イベントを受け取ったら
// 対応するEffectInstanceへEmitを行う。
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

	// コピー禁止
	// ※EffectInstance(コピー禁止)を値として持つunordered_map、および購読の自動解除用
	//   ScopedSubscriberのvectorを持つ為、値としてコピーされる状況自体を作らない。
	EffectDispatcher(const EffectDispatcher&) = delete;
	EffectDispatcher& operator=(const EffectDispatcher&) = delete;

	// busはシーンのライフサイクルに合わせて破棄されるバスを渡す想定
	// effectDataPathはEffectEditorが保存するJSONと同じものを指定する(既定はEditor側の初期パスと同じ)
	bool Init(EventBus& bus, const std::string& effectDataPath = "Asset/Data/Game/effectmap.json");

	void Release();

	// 生きている全エフェクトの更新のみ行う(発生はイベント経由でのみ起こる)
	void Update(float deltaTime);

	// 保持している全エフェクトの描画
	void Draw();

private:

	// GenericEffectSpawnEventを受け取った時の処理
	void OnGenericEffectSpawn(const Events::Effect::GenericEffectSpawnEvent& e);

	// WeaponClashEffectEventを受け取った時の処理
	void OnWeaponClash(const Events::Effect::WeaponClashEffectEvent& e);

	// effectDataPathをEffectDataLoaderで読み込み、m_simpleEffectsとm_weaponClashParamsを構築する
	// (JSONが無い/読み込み失敗時はどちらもデフォルト値のまま：単純エフェクトは1つも
	//  登録されなくなるが、鍔迫り合いの火花は元のハードコード値と同じ見た目で動作する)
	bool LoadEffectData(const std::string& effectDataPath);

	// EffectId → 実行時インスタンス
	//	生成・Gravity込みUpdate・テクスチャ込みDrawはEffectInstance内部に閉じている
	std::unordered_map<EffectId, EffectInstance> m_simpleEffects;

	// テクスチャ取得(EffectInstance用。EffectEditorと同じ実装を共有する)
	KdAssetsTextureProvider m_textureProvider;

	// 鍔迫り合いの火花専用(EffectInstanceの対象範囲外。理由はクラスコメント参照)
	std::shared_ptr<KdGPUParticle>	m_clashSparkParticle;
	std::shared_ptr<KdTexture>		m_clashSparkTexture;
	WeaponClashEffectParams			m_weaponClashParams;

	// 購読の自動解除用(Release時にまとめて解除される)
	std::vector<ScopedSubscriber> m_subscriptions;

	EventBus* m_bus = nullptr;
};