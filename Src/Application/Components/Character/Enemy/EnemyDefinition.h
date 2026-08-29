#pragma once
#include <string>
#include "EnemyStatusData.h"

// ============================================================
// 敵1種類分のパラメータ。CSV/JSON等の外部データから読み込んで
// database(std::unordered_map<std::string, EnemyDefinition>)を
// 組み立てる想定(EnemyFactoryのコンストラクタ参照)。
//
// 実際のJSON等からの読み込み処理はまだ実装していないため、当面は
// EnemyDefinitionDatabase.h側のCreateDebugEnemyDatabase()で
// コード上に直書きしたデータを使う(読み込み処理が決まったら、
// そちらの戻り値をこのdatabaseに差し替えるだけで良いようにしてある)。
// ============================================================
struct EnemyDefinition
{
	// どの派生クラス(EnemyStatusControllerの派生)を生成するか。
	enum class EnemyType
	{
		Brute, // 雑魚敵
		Boss,  // ボス(名称未定)
	};

	std::string name = "Enemy"; // GameObjectの表示名
	EnemyType type = EnemyType::Brute;

	// SkeletonComponent::SetModelData()にそのまま渡すパス。以前は
	// EnemyFactory::BuildEnemy()内で決め打ちだったが、敵の種類ごとに
	// 見た目のモデルが変わるようになったためこちらへ出した。
	std::string modelPath = "Asset/Models/Character/Brute/Brute.gltf";

	float moveSpeed = 1.5f;
	float bodyRadius = 0.5f; // 被弾判定(Hurtbox)の球半径

	// パトロール距離・攻撃タイマー・ノックバッククランプ等、
	// EnemyStatusController(および派生クラス)向けのチューニング値。
	// 以前はpatrolDistanceを単独フィールドとして持っていたが、
	// EnemyStatusData側に統合したため、ここでは重複して持たない。
	// 詳細はEnemyStatusData.h参照。
	EnemyStatusData statusData;
};
