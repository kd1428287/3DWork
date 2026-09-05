#pragma once

#include "../Effect/EffectEvents.h"
#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"
#include "../Effect/EffectDataLoader.h"
#include "../Effect/EffectInstance.h"
#include "../Effect/KdAssetsTextureProvider.h"

#include <unordered_map>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト生成ディスパッチャー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// シーン単位のEventBusを購読し、エフェクト生成イベントを受け取ったら
// 対応するEffectInstanceへEmitを行う。
//
// ・座標だけで足りる単純なエフェクト(HitSpark等) → GenericEffectSpawnEvent
// ・鍔迫り合いの火花(方向が要る)                  → WeaponClashEffectEvent
// ・動く発生源に追従させて継続再生したいエフェクト  → EffectAttachSpawnEvent〜EffectDetachEvent
//   (松明の火の粉、キャラクター追従の砂煙等。Continuous、または再発生ありのBurst向け)
//
// 鍔迫り合いの火花も専用のデータ型・専用のKdGPUParticleは持たず、
// "WeaponClashParry"/"WeaponClashBlock"という名前の通常のエフェクト定義(std::string対応)として
// simpleEffects_に統合されている。
// ・OnGenericEffectSpawn()：simpleEffects_からIDを引いてEmit(position)するだけ(baseDir無し)
// ・OnWeaponClash()       ：simpleEffects_からIsParryに応じたIDを引いてEmit(position, baseDir)する
//                           (baseDirの計算だけがこのイベント固有の処理として残る)
//
// 【simpleEffects_ と activeInstances_ の違い】
//   simpleEffects_   ：JSON上の各エフェクト名につき「使い回し1体」のテンプレートインスタンス。
//                       Emit()を都度呼ぶだけの単発発生専用(同時に複数の発生源を持てない)。
//   activeInstances_ ：EffectAttachSpawnEventで生成される、InstanceKeyごとに独立した
//                       実行時インスタンス。Play()され、EffectPositionUpdateEventで
//                       毎フレーム位置を追従させ、EffectDetachEventで破棄される。
//                       複数の発生源(例えば松明が複数)を同時に、それぞれ別の位置で
//                       継続再生したい場合はこちらを使う。
//
// Init()時にEffectDataLoaderでJSON(effectDataPath)を読み込んで構築する
// (EffectEditorが保存したものと同じファイルを指定すれば、エディタで調整した
// パラメータがそのままゲーム実行時にも反映される)。
// JSON側の"name"がstd::string名と一致する項目のみ登録される
// (WeaponClashParry/WeaponClashBlockがJSONに定義されていない場合、鍔迫り合いイベントを
//  受け取っても何も発生しない。コード内蔵のデフォルト値によるフォールバックは持たない点に注意)
//
// 【使い方】
//   EffectDispatcher dispatcher;
//   dispatcher.Init(sceneLocalEventBus, "Asset/Data/Game/effectmap.json");
//   :
//   // 毎フレーム
//   dispatcher.Update(deltaTime);
//   dispatcher.Draw();
//   :
//   // 継続再生させたい発生源がある場合(例：松明を生成した時)
//   PublishEffectAttach(bus, "Torch_003", "TorchFire", torchPos);
//   // 毎フレーム(発生源が動く場合)
//   PublishEffectPositionUpdate(bus, "Torch_003", torchPos);
//   // 発生源が消える時
//   PublishEffectDetach(bus, "Torch_003");
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectDispatcher
{
public:

	EffectDispatcher() {}
	~EffectDispatcher() { Release(); }

	// コピー禁止
	// ※EffectInstance(コピー禁止)を値として持つunordered_mapを2つ、および購読の自動解除用
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
	//	baseDirの計算のみここで行い、Emit自体はsimpleEffects_(WeaponClashParry/Block)に委譲する
	void OnWeaponClash(const Events::Effect::WeaponClashEffectEvent& e);

	// 継続再生の開始/位置更新/終了
	void OnEffectAttachSpawn(const Events::Effect::EffectAttachSpawnEvent& e);
	void OnEffectPositionUpdate(const Events::Effect::EffectPositionUpdateEvent& e);
	void OnEffectDetach(const Events::Effect::EffectDetachEvent& e);

	// effectDataPathをEffectDataLoaderで読み込み、simpleEffects_を構築する
	// (JSONが無い/読み込み失敗時は空のまま：単純エフェクトも鍔迫り合いの火花も
	//  1つも発生しなくなる)
	bool LoadEffectData(const std::string& effectDataPath);

	// std::string → 実行時インスタンス(単発発生用の使い回しテンプレート)
	//	生成・Gravity込みUpdate・テクスチャ込みDrawはEffectInstance内部に閉じている。
	//	単純エフェクトと鍔迫り合いの火花(Parry/Block)の両方がここに同居する。
	std::unordered_map<std::string, EffectInstance> simpleEffects_;

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// 継続再生中のインスタンス(InstanceKey → 実行時状態)
	//	simpleEffects_とは別に、Attachされた分だけ独立したEffectInstanceを持つ。
	//	positionは発生源からEffectPositionUpdateEventで送られてくる最新値を保持しておき、
	//	Update()で毎フレームEffectInstance::Update(deltaTime, position)に渡す。
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	struct ActiveInstance
	{
		EffectInstance					instance;
		DirectX::SimpleMath::Vector3	position = { 0, 0, 0 };
	};
	std::unordered_map<std::string, ActiveInstance> activeInstances_;

	// テクスチャ取得(EffectInstance用。EffectEditorと共通の実装)
	KdAssetsTextureProvider textureProvider_;

	// 購読の自動解除用(Release時にまとめて解除される)
	std::vector<ScopedSubscriber> subscriptions_;

	EventBus* bus_ = nullptr;
};