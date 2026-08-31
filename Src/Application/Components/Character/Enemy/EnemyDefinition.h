#pragma once
#include <string>
#include "EnemyAIData.h"

// ============================================================
// 敵1種類分のパラメータ。CSV/JSON等の外部データから読み込んで
// database(std::unordered_map<std::string, EnemyDefinition>)を
// 組み立てる想定(EnemyFactoryのコンストラクタ参照)。
//
// 【変更】以前はEnemyType(Brute/Boss)で生成する派生クラス
// (BruteStatusController/BossStatusController)を出し分けていたが、
// 継承ベースの実行層を全面的に廃止し、単一のEnemyAIController
// (データ駆動)に統合したため、EnemyTypeという分岐自体が不要になった。
// 敵種の違いは全てaiData(EnemyAIData)の中身の違いとして表現する
// (EnemyAIData::CreateDebugBruteAIData()/CreateDebugBossAIData()参照)。
// ============================================================
struct EnemyDefinition
{
	std::string name = "Enemy"; // GameObjectの表示名

	// SkeletonComponent::SetModelData()にそのまま渡すパス。敵の種類ごとに
	// 見た目のモデルが変わる想定。
	std::string modelPath = "Asset/Models/Character/Brute/Brute.gltf";

	float moveSpeed = 1.5f;
	float bodyRadius = 0.5f; // 被弾判定(Hurtbox)の球半径

	// 意思決定・攻撃パターン等、EnemyAIController向けのチューニング値。
	// 敵の種類ごとの違いは全てこのデータの中身で表現する。
	EnemyAIData aiData;
};
