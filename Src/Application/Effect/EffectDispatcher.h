#pragma once

#include "../Effect/EffectEvents.h"
#include "../../Framework/Shader/GPUParticle/KdGPUParticle.h"
#include "../Effect/EffectDataLoader.h"
#include "../Effect/EffectInstance.h"
#include "../Effect/KdAssetsTextureProvider.h"

#include <unordered_map>
#include <algorithm>

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
// 【実行中のホットリロードについて】
//   EffectEditorでSaveした瞬間、実行中の(同じJSONパスを読み込んでいる)EffectDispatcher
//   全てへ、静的関数NotifyDataSaved()経由で再ロードを促す。EffectEditorはEventBusを
//   経由せず、生存中のEffectDispatcherインスタンス一覧(s_instances)をパス一致で
//   直接辿って呼び出す(EffectEditorはシーンに属さない編集用シングルトンで、
//   どのシーンのEventBusに繋げばよいか分からない為、EventBus越しの通知にはしていない)。
//   ※現在Attach中のactiveInstances_(継続再生中のインスタンス)はそれぞれ既に
//     パラメータのコピーを持っている為、再ロードしても見た目は変わらない
//     (次にAttachし直した時から新しいパラメータになる)。simpleEffects_(単発発生用の
//     テンプレート)は次のEmit()から即座に新しいパラメータが反映される。
//
// 【使い方】
//   EffectDispatcher dispatcher;
//   dispatcher.Init(sceneLocalEventBus, "Asset/Data/Game/effectmap.json");
//   :
//   dispatcher.Update(deltaTime);            // 毎フレーム1回
//   :
//   // 3D描画パス側、DrawLitとDrawBloomの両方から、それぞれ対応するpassで呼ぶ
//   // (DrawPass::LitのエフェクトはDrawLit側の呼び出しでのみ、Bloomのエフェクトは
//   //  DrawBloom側の呼び出しでのみ実際に描画される)
//   dispatcher.Draw(KdParticleDrawPass::Lit);      // DrawLit内から
//   dispatcher.Draw(KdParticleDrawPass::Bloom);    // DrawBloom内から
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

	// 保持している全エフェクトの描画。
	//	DrawPassFlagsにpassが含まれるEffectInstanceだけが実際に描画される。
	//	3D描画パス側のDrawLit・DrawBloomそれぞれから、対応するpassで1回ずつ
	//	毎フレーム呼んでもらう想定(両方のフラグを持つエフェクトは両方から描画される)
	void Draw(ParticleDrawPass pass);

	// このインスタンスが読み込んでいるJSONパス
	const std::string& GetEffectDataPath() const { return effectDataPath_; }

	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	// pathと同じJSONを読み込んでいる、現在生きている全EffectDispatcherインスタンスへ
	// 再ロードを促す(EffectEditor::Save()が保存成功時に呼ぶ想定)。
	// 該当インスタンスが1つも無い場合(実行中でない、または別のパスを見ている場合)は何もしない。
	// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
	static void NotifyDataSaved(const std::string& path);

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
	// ※NotifyDataSaved()からも呼ばれる(実行中の再ロード)
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

	// Init()時に渡されたJSONパス(NotifyDataSaved()での再ロード対象判定、および
	// GetEffectDataPath()用に保持しておく)
	std::string effectDataPath_;

	// 生存中の全EffectDispatcherインスタンス(NotifyDataSaved()がここを辿ってpath一致のものへ
	// 再ロードを促す)。Init()で登録、Release()で解除する
	static std::vector<EffectDispatcher*> s_instances;
};