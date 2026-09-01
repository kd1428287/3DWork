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
//
// 【再追加】Warrockのように、パラメータの違いだけでなくロジック
// (BuildTree()の構造そのもの)がEnemyAIControllerと完全に異なる敵種を
// 追加したため、「どのAIControllerをアタッチするか」を選ぶ種別として
// EnemyTypeを復活させた。以前との違いは、継承ベースの実行クラスを
// 出し分けるためではなく、EnemyFactory::CreateAIController()が
// AddComponentする「型」を選ぶためだけに使う点(EnemyFactory.cpp参照)。
// ============================================================

// EnemyFactory::CreateAIController()がこれを見てAddComponentする型を
// 切り替える。新しい敵種(専用AIControllerを持つもの)を追加する場合は
// ここに値を足し、CreateAIController()のswitchにケースを足すこと。
enum class EnemyType
{
	Brute,   // 汎用EnemyAIController(データ駆動、雑魚敵向け)
	Warrock, // Warrock専用WarrockAIController(BuildTree()自体が固有)
};

struct EnemyDefinition
{
	std::string name = "Enemy"; // GameObjectの表示名

	// SkeletonComponent::SetModelData()にそのまま渡すパス。敵の種類ごとに
	// 見た目のモデルが変わる想定。
	std::string modelPath = "Asset/Models/Character/Brute/Brute.gltf";

	// どのAIControllerをアタッチするか(EnemyFactory::CreateAIController()
	// 参照)。デフォルトは既存の汎用EnemyAIController。
	EnemyType type = EnemyType::Brute;

	float moveSpeed = 1.5f;
	float bodyRadius = 0.5f; // 被弾判定(Hurtbox)の球半径
	Math::Vector3 modelScale = { 1.f,1.f,1.f };

	// 意思決定・攻撃パターン等、AIController向けのチューニング値。
	// EnemyAIData型自体はどの敵種でも共用する「データの器」であり、
	// これを解釈するロジック(BT)側の共有は意味しない
	// (WarrockAIData.h冒頭コメント参照)。
	EnemyAIData aiData;
};