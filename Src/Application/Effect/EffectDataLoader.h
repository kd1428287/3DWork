#pragma once

#include "EffectParams.h"
#include "../Effect/EffectEvents.h"	// EffectId ※実際の配置パスに合わせて調整
#include <vector>
#include <string>

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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// エフェクト定義JSON全体(1ファイル分)のデータ
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 全てのエフェクト(座標だけで足りる単純エフェクトも、鍔迫り合いの火花のように
// 発生方向を外部から受け取るエフェクトも)を、汎用のGPUParticleParams(Layers構成)で
// 表現された単一のEffects一覧としてまとめる。
// 以前あった専用の"weaponClash"キー・専用データ型(WeaponClashEffectParams等)は廃止した。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct EffectDataFile
{
	std::vector<EffectDefinition>	Effects;
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
};