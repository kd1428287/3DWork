#pragma once

#include "EffectParams.h"
#include "../Engine/EventBus/Event/EffectEvents.h"	// EffectId ※実際の配置パスに合わせて調整
#include <vector>
#include <string>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// JSON上の1エフェクト定義に対応するデータ
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EffectEditor(配置・プレビュー編集)とEffectDispatcher(実行時のEmit)の
// 両方が読み込む共通の中間データ。
// ・Name  ：識別名。EffectDispatcherではEffectId名(HitSpark等)と一致させて紐付ける
// ・Params：発生パラメータ本体(GPUParticleParams)
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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト定義JSON全体(1ファイル分)のデータ
//	Effects    ：座標だけで足りる単純エフェクト(EffectId対応)の配置一覧
//	WeaponClash：鍔迫り合いの火花(専用エフェクト、マップ配置ではなく単一のグローバル設定)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EffectDataFile
{
	std::vector<EffectDefinition>	Effects;
	WeaponClashEffectParams		WeaponClash;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト定義JSONの読み書き専用ラッパー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 汎用のKdJsonLoaderを使ってファイルI/Oを行い、EffectDataFile⇔JSONの
// スキーマ変換(このファイル固有の知識)のみをここに閉じ込める。
// EffectEditor/EffectDispatcherはEffectDataFileだけを受け取り、
// それぞれ自分の内部表現(EffectObject／SimpleEffectEntry等)に変換して使う。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class EffectDataLoader
{
public:

	// pathからエフェクトデータ一式を読み込む。失敗時はfalseを返しoutは変更しない
	static bool Load(const std::string& path, EffectDataFile& out);

	// エフェクトデータ一式をpathへ書き出す
	static bool Save(const std::string& path, const EffectDataFile& data);

	// Name文字列 ⇔ EffectId (Dispatcher側の対応表構築で使用)
	static bool NameToEffectId(const std::string& name, EffectId& out);
	static std::string EffectIdToName(EffectId id);
};