#pragma once

#include <optional>

#include "../Effect/EffectParams.h"
#include "Application/Core/EventBus/Events/EffectEvents.h"	// EffectId ※実際の配置パスに合わせて調整

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// JSON上の1エフェクト定義に対応するデータ
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EffectEditor(配置・プレビュー編集)とEffectDispatcher(実行時のEmit)の
// 両方が読み込む共通の中間データ。
// ・Name  ：識別名。EffectDispatcherではEffectId名(HitSpark等)と一致させて紐付ける
//   ※鍔迫り合いの火花も"WeaponClashParry"/"WeaponClashBlock"という名前の
//   通常のエフェクト定義として、ここに含まれる(専用のデータ型は廃止した)
// ・Params：発生パラメータ本体(GPUParticleParams、Layers構成)
// ・Pos/Rotate/Scale：EffectEditorでのマップ配置用(Dispatcherでは未使用。
//   実行時の発生位置はイベント側のPositionを使う)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EffectDefinition
{
	std::string			Name;
	GPUParticleParams	Params;

	DirectX::SimpleMath::Vector3	Pos = { 0,0,0 };
	DirectX::SimpleMath::Vector3	Rotate = { 0,0,0 };
	DirectX::SimpleMath::Vector3	Scale = { 1,1,1 };
};

// 墨(BlendMode=LiquidInk)の質感パラメータ。全エフェクト共通の1セットで、JSONでは"liquidInk"キーに保存する。
// 既定値はKdPostProcessShader::cbLiquidInfoと同じ値に揃えてある
struct LiquidInkSettings
{
	DirectX::SimpleMath::Vector3	InkColor = { 0.10f, 0.10f, 0.11f };
	DirectX::SimpleMath::Vector3	EdgeColor = { 0.01f, 0.01f, 0.01f };
	float	Threshold = 0.35f;
	float	Softness = 0.05f;
	float	EdgeWidth = 0.15f;
	float	HaloAlpha = 0.25f;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト定義JSON全体(1ファイル分)のデータ
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 全てのエフェクト(座標だけで足りる単純エフェクトも、鍔迫り合いの火花のように
// 発生方向を外部から受け取るエフェクトも)を、汎用のGPUParticleParams(Layers構成)で
// 表現された単一のEffects一覧としてまとめる。
// 以前あった専用の"weaponClash"キー・専用データ型(WeaponClashEffectParams等)は廃止した。
//
// Groups：グループ名 → メンバーのEffect名一覧。実体は持たず、Effects内の名前を
//	参照するだけの間接参照層(テクスチャ・BlendMode等が異なる複数のエフェクトを
//	1つの名前でまとめて発生させたい場合に使う。輪+火花のような組み合わせを想定)。
//	メンバー名がEffectsに実在するかはロード時点ではチェックしない
//	(EffectDispatcher側がEmit解決時に「存在すれば発生、無ければ無視」する方針の為。
//	 保存順序によって一時的に参照切れになるケースを誤ってエラー扱いしないようにしている)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EffectDataFile
{
	std::vector<EffectDefinition>	Effects;
	std::unordered_map<std::string, std::vector<std::string>>	Groups;

	// 墨の質感(全体設定)。JSONに"liquidInk"が無ければ未設定(反映も保存もしない)
	std::optional<LiquidInkSettings>	LiquidInk;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト定義JSONの読み書き専用ラッパー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 汎用のKdJsonLoaderを使ってファイルI/Oを行い、EffectDataFile⇔JSONの
// スキーマ変換(このファイル固有の知識)のみをここに閉じ込める。
// EffectEditor/EffectDispatcherはEffectDataFileだけを受け取り、
// それぞれ自分の内部表現(EffectObject／EffectInstanceの対応表等)に変換して使う。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectDataLoader
{
public:

	// pathからエフェクトデータ一式を読み込む。失敗時はfalseを返しoutは変更しない
	static bool Load(const std::string& path, EffectDataFile& out);

	// エフェクトデータ一式をpathへ書き出す
	static bool Save(const std::string& path, const EffectDataFile& data);

	// 墨の質感設定とポストプロセス側の現在値の受け渡し(Save前の取得 / Load後の反映用)
	static LiquidInkSettings CaptureLiquidInk();
	static void ApplyLiquidInk(const LiquidInkSettings& settings);
};