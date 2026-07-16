#pragma once
#include <string>

// ============================================================
// 敵1種類分のパラメータ。CSV/JSON等の外部データから読み込んで
// database(std::unordered_map<std::string, EnemyDefinition>)を
// 組み立てる想定(EnemyFactoryのコンストラクタ参照)。
//
// 今はパトロールAI向けの最小構成のみ。HP・攻撃力・使用モデル等は
// 実際に必要になった時点でここへフィールドを追加していけばよい。
// ============================================================
struct EnemyDefinition
{
	std::string name = "Enemy"; // GameObjectの表示名
	float patrolDistance = 3.0f; // 基準点からどれだけ離れたら折り返すか
	float moveSpeed = 1.5f;
	float bodyRadius = 0.5f; // 被弾判定(Hurtbox)の球半径
};