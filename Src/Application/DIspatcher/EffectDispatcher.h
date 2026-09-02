#pragma once

#include "../Engine/EventBus/Event/EffectEvents.h"
#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト生成ディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、エフェクト生成イベントを受け取ったら
// 対応するKdGPUParticleへEmitを行う。
//
// ・座標だけで足りる単純なエフェクト(HitSpark等) → GenericEffectSpawnEvent
//   → EffectId→発生設定の対応表(m_simpleEffects)を引いて処理
// ・データの形が違う専用エフェクト(鍔迫り合いの火花等) → 専用の具象イベント型
//   → 専用のハンドラ・専用のKdGPUParticleを個別に持つ
//
// 【使い方】
//   EffectDispatcher dispatcher;
//   dispatcher.Init(sceneLocalEventBus);
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
	bool Init(EventBus& bus);

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

	// EffectId → 発生設定
	std::unordered_map<EffectId, SimpleEffectEntry> m_simpleEffects;

	// 鍔迫り合いの火花専用
	std::shared_ptr<KdGPUParticle>	m_clashSparkParticle;
	std::shared_ptr<KdTexture>		m_clashSparkTexture;

	// 購読の自動解除用(Release時にまとめて解除される)
	std::vector<ScopedSubscriber> m_subscriptions;

	EventBus* m_bus = nullptr;
};
