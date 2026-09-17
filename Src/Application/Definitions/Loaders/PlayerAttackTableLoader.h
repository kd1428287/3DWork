#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"

// ============================================================
// PlayerAttackTable(コンボ木)専用のファイルI/O。
//
// 【PlayerDefinitionLoaderから分離した理由】
// 攻撃1技あたりのデータ(ダメージ/体幹/秒数/フレーム範囲/ステップ移動/
// キャンセル受付/武器スロット...)は今後さらに項目が増え、かつ技数自体も
// 増えていく前提のデータ(Player.json全体の中では突出して肥大化しやすい)。
// Player.json本体(見た目・数値・コライダー・ソケット・武器等)と混ぜて
// 1ファイルに置き続けると、コンボ調整のたびに無関係な項目まで含む
// 巨大な差分が発生し、レビューもしづらくなる。そのため専用ファイル
// (例: "Asset/Data/Player/PlayerAttackTable.json")に切り出し、
// このローダーが単体で読み書きする。
//
// PlayerDefinitionLoaderは、Player.json側のcombatBehavior.attackTablePath
// (このファイルへのパス)を読み取り、このLoadFromFile()へ委譲する
// (PlayerDefinitionLoader.cpp::ReadCombatBehavior参照)。
// ============================================================
namespace PlayerAttackTableLoader
{
	// path 例: "Asset/Data/Player/PlayerAttackTable.json"
	// 読み込み・パースに失敗した場合はfalseを返し、outTableは変更しない。
	bool LoadFromFile(const std::string& path, PlayerAttackTable& outTable);
}
