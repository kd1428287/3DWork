#pragma once

#include <mutex>
#include <atomic>
#include <variant>

#include "Application/Core/EventBus/Events/EffectEvents.h"
#include "Framework/Shader/GPUParticle/ParticleBuffer.h"
#include "Application/Definitions/Loaders/EffectDataLoader.h"
#include "EffectInstance.h"
#include "../Common/KdAssetsTextureProvider.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト生成ディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、エフェクト生成イベントを受け取ったら
// 対応するEffectInstanceへEmitを行う。
//
// ・座標＋方向で表現できるエフェクト(HitSpark等の単純なものも、鍔迫り合いの火花のような
//   方向を伴うものも含む) → GenericEffectSpawnEvent。方向計算(反射方向の近似等)は
//   Publish側(HitReactionComponent等)が済ませてから渡す想定で、Dispatcher側では
//   「対応表を引いてEmitするだけ」に徹する
// ・動く発生源に追従させて継続再生したいエフェクト  → EffectAttachSpawnEvent〜EffectDetachEvent
//   (松明の火の粉、キャラクター追従の砂煙等。Continuous、または再発生ありのBurst向け)
//
// 鍔迫り合いの火花も専用のイベント型・専用のParticleBufferは持たず、
// "WeaponClashParry"/"WeaponClashBlock"という名前の通常のエフェクト定義(std::string対応)として
// simpleEffects_に統合されている(GenericEffectSpawnEvent::Idとして渡ってくる)。
//
// 【simpleEffects_ と activeInstances_ の違い】
//   simpleEffects_   ：JSON上の各エフェクト名につき「使い回し1体」のテンプレートインスタンス。
//                       Emit()を都度呼ぶだけの単発発生専用(同時に複数の発生源を持てない)。
//   activeInstances_ ：EffectAttachSpawnEventで生成される、InstanceKeyごとに独立した
//                       実行時インスタンス。Play()され、EffectPositionUpdateEventで
//                       毎フレーム位置を追従させ、EffectDetachEventで破棄される。
//
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 【イベント処理のタイミングについて(重要)】
//	OnXxx系(イベント購読のコールバック)は、Publish()を呼んだ側のコールスタックの中で
//	そのまま実行される。OnXxx系はpendingEvents_(mutexで保護)に「発生リクエスト」を
//	積むだけに留め、simpleEffects_/activeInstances_の読み書き・GPUコマンド発行は全て
//	Update()冒頭のProcessPendingEvents()に一本化している。これにより：
//	・Publish()は任意のスレッドから呼んでよい
//	・simpleEffects_/activeInstances_は常にUpdate()を呼ぶスレッドからのみ変更される
//	・NotifyDataSaved()も同様にreloadRequested_を立てるだけにし、実際のLoadEffectData()は
//	  Update()冒頭で行う(＝Save直後ではなく次のUpdate()まで1フレーム遅延する)
//
//	※ただしUpdate()自体、およびDraw()は、引き続き同一スレッド(D3D11イミディエイト
//	  コンテキストを握るスレッド)から呼ばれる前提。この2つを別スレッドから並行に
//	  呼ぶことは想定していない
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//
// 【使い方】
//   EffectDispatcher dispatcher;
//   dispatcher.Init(sceneLocalEventBus, "Asset/Data/Game/effectmap.json");
//   :
//   dispatcher.Update(deltaTime);            // 毎フレーム1回(先頭でpending処理も行う)
//   :
//   dispatcher.Draw(KdParticleDrawPass::Lit);      // DrawLit内から
//   dispatcher.Draw(KdParticleDrawPass::Bloom);    // DrawBloom内から
//   :
//   PublishEffectAttach(bus, "Torch_003", "TorchFire", torchPos);
//   PublishEffectPositionUpdate(bus, "Torch_003", torchPos);
//   PublishEffectDetach(bus, "Torch_003");
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectDispatcher
{
public:

	EffectDispatcher() {}
	~EffectDispatcher() { Release(); }

	EffectDispatcher(const EffectDispatcher&) = delete;
	EffectDispatcher& operator=(const EffectDispatcher&) = delete;

	bool Init(EventBus& bus, const std::string& effectDataPath = "Asset/Data/Game/effectmap.json");

	void Release();

	// pending(発生・Attach・位置更新・Detach・データ再ロード)の消化 → 生きている全エフェクトの更新
	void Update(float deltaTime);

	void Draw(ParticleDrawPass pass);

	const std::string& GetEffectDataPath() const { return effectDataPath_; }

	// pathと同じJSONを読み込んでいる全EffectDispatcherへ、再ロード要求を立てる(即時ロードはしない)。
	// 実際のLoadEffectData()は各インスタンスの次回Update()冒頭で行われる
	static void NotifyDataSaved(const std::string& path);

private:

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 発生リクエスト(イベントハンドラはこれを積むだけ。simpleEffects_/activeInstances_には触れない)
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	struct PendingSpawn
	{
		std::string						Id;
		DirectX::SimpleMath::Vector3	Position;
		DirectX::SimpleMath::Vector3	BaseDir = { 0, 0, 0 };
	};
	struct PendingAttach
	{
		std::string						InstanceKey;
		std::string						EffectName;
		DirectX::SimpleMath::Vector3	Position;
	};
	struct PendingPositionUpdate
	{
		std::string						InstanceKey;
		DirectX::SimpleMath::Vector3	Position;
	};
	struct PendingDetach
	{
		std::string	InstanceKey;
	};
	using PendingEvent = std::variant<PendingSpawn, PendingAttach, PendingPositionUpdate, PendingDetach>;

	// イベントハンドラ：pendingEvents_へ積むだけ(mutexで保護)
	void OnGenericEffectSpawn(const Events::Effect::GenericEffectSpawnEvent& e);
	void OnEffectAttachSpawn(const Events::Effect::EffectAttachSpawnEvent& e);
	void OnEffectPositionUpdate(const Events::Effect::EffectPositionUpdateEvent& e);
	void OnEffectDetach(const Events::Effect::EffectDetachEvent& e);

	// pendingEvents_を取り出して1件ずつ処理する(Update()冒頭から呼ばれる)
	void ProcessPendingEvents();

	bool LoadEffectData(const std::string& effectDataPath);

	std::unordered_map<std::string, EffectInstance> simpleEffects_;

	struct ActiveInstance
	{
		EffectInstance					instance;
		DirectX::SimpleMath::Vector3	position = { 0, 0, 0 };
	};
	std::unordered_map<std::string, ActiveInstance> activeInstances_;

	KdAssetsTextureProvider textureProvider_;

	std::vector<ScopedSubscriber> subscriptions_;

	EventBus* bus_ = nullptr;

	std::string effectDataPath_;

	// NotifyDataSaved()が立てるだけの再ロード要求フラグ。実処理はUpdate()冒頭で行う
	std::atomic<bool> reloadRequested_ = false;

	// イベントハンドラ(任意スレッド)とUpdate()(所有スレッド)の間の受け渡し用
	std::mutex					pendingMutex_;
	std::vector<PendingEvent>	pendingEvents_;

	// 生存中の全EffectDispatcherインスタンス(NotifyDataSaved()用)。s_instancesMutexで保護する
	static std::vector<EffectDispatcher*> s_instances;
	static std::mutex s_instancesMutex;
};