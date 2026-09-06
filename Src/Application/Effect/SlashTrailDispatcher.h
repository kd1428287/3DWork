#pragma once

#include "SlashTrailEvents.h"
#include "SlashTrailInstance.h"

#include <unordered_map>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)生成ディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、SlashTrailBegin/PositionUpdate/Endの3イベントを受け取って
// SlashTrailInstanceの記録開始/位置更新/記録終了を行う。EffectDispatcherと対になる、
// 面エフェクト(動的頂点)側のディスパッチャー。
//
// 【EffectDispatcherとの構造上の違い】
//	・GPUパーティクルのsimpleEffects_は「名前ごとに使い回す1体のテンプレート」に
//	  Emit()するだけだったが、トレイルは記録開始のたびに"新しいインスタンス"が要る
//	  (同時に複数本の剣が同時に軌跡を引きうる為)。よってactiveTrails_は
//	  InstanceKeyごとに新規生成される、EffectDispatcher::activeInstances_と同じ形になる
//	・GPUパーティクルはDetach即破棄(EffectDispatcher::OnEffectDetach)で問題なかったが、
//	  トレイルはEndRecording()後もフェードアウトが終わるまで描画し続ける必要がある為、
//	  Update()の中でSlashTrailInstance::IsFinished()を確認してから初めて破棄する
//	  「刈り取り」処理が要る(ここがEffectDispatcherに無かった処理)
//
// 【trailDefinitions_について】
//	現時点ではJSON読み込みは未実装で、RegisterDefinition()でコードから直接登録する想定。
//	EffectDataLoaderのようなJSON化は、面エフェクト用のエディタに着手するタイミングで
//	同じパターン(name→JSON変換)を踏襲して追加すること
//
// 【使い方】
//   SlashTrailDispatcher dispatcher;
//   dispatcher.Init(sceneLocalEventBus);
//   dispatcher.RegisterDefinition("PlayerSwordTrail", params);
//     :
//   // 毎フレーム
//   dispatcher.Update(deltaTime);
//   dispatcher.Draw(KdParticleDrawPass::Lit);      // DrawLit内から
//   dispatcher.Draw(KdParticleDrawPass::Bloom);    // DrawBloom内から
//     :
//   // 武器側コンポーネントが発行する想定(SlashTrailDispatcherの存在は知らない)
//   PublishSlashTrailBegin(bus, "Sword_Player", "PlayerSwordTrail");
//   PublishSlashTrailPositionUpdate(bus, "Sword_Player", tipPos, basePos);
//   PublishSlashTrailEnd(bus, "Sword_Player");
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class SlashTrailDispatcher
{
public:

	SlashTrailDispatcher() {}
	~SlashTrailDispatcher() { Release(); }

	// コピー禁止(SlashTrailInstanceがコピー禁止の為、値として持つunordered_mapを介して
	// コピーされる状況自体を作らない。EffectDispatcherと同じ理由)
	SlashTrailDispatcher(const SlashTrailDispatcher&) = delete;
	SlashTrailDispatcher& operator=(const SlashTrailDispatcher&) = delete;

	// busはシーンのライフサイクルに合わせて破棄されるバスを渡す想定
	bool Init(EventBus& bus);

	void Release();

	// name(トレイル定義名)でSlashTrailParamsを登録する。同名で再登録すると上書きされる。
	//	以後のSlashTrailBeginEvent(TrailName一致分)はこの定義を参照するようになる
	void RegisterDefinition(const std::string& name, const SlashTrailParams& params);

	// 全アクティブトレイルの更新。
	//	各インスタンスのUpdate(deltaTime)を呼んだ後、IsFinished()==trueになったものは
	//	ここで破棄する(EndRecording直後ではなく、フェードアウトし切ってから破棄する点に注意)
	void Update(float deltaTime);

	// DrawPassFlagsにpassが含まれるアクティブトレイルだけを描画する
	//	(EffectDispatcher::Draw(pass)と同じ規約。DrawLit/DrawBloomの両方から呼ぶ想定)
	void Draw(ParticleDrawPass pass);

private:

	void OnSlashTrailBegin(const Events::SlashTrail::SlashTrailBeginEvent& e);
	void OnSlashTrailPositionUpdate(const Events::SlashTrail::SlashTrailPositionUpdateEvent& e);
	void OnSlashTrailEnd(const Events::SlashTrail::SlashTrailEndEvent& e);

	// name → 定義(テンプレート)。SlashTrailBeginEventで新規SlashTrailInstanceを作る時に参照する
	std::unordered_map<std::string, SlashTrailParams> trailDefinitions_;

	// InstanceKey → アクティブなトレイル本体。
	//	EndRecording後もIsFinished()になるまでここに残り続ける(Update()が刈り取るまで)
	std::unordered_map<std::string, SlashTrailInstance> activeTrails_;

	// 購読の自動解除用(Release時にまとめて解除される)
	std::vector<ScopedSubscriber> subscriptions_;

	EventBus* bus_ = nullptr;
};
