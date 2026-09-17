#pragma once
#include "EnemyAIData.h"
#include "../Common/CharacterCollisionDefaults.h"

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
//
// 【moveSpeedの削除について】
// 以前はEnemyAIData::patrolSpeed/chaseSpeedとは別にmoveSpeedを
// 独立して持っていたが、実行層(EnemyActions.cpp/WarrockActions.cpp)の
// どこからも参照されておらず、初期動作確認時の名残と判断して削除した。
//
// 【bodyRadius/modelScale→characterScaleについて】
// 以前は「Hurtbox半径(bodyRadius)」と「見た目スケール(modelScale)」を
// 独立した値として持っていたため、敵ごとに手打ちすると「見た目は
// 大きいのに当たり判定はデフォルトのまま」のようなズレが起きえた。
// CharacterCollisionDefaults.h冒頭コメントで予告されていた「将来、
// 体格の異なる敵種が増えたらEnemyDefinition側に身長・footOffset相当の
// フィールドを持たせる」という拡張を、単一の倍率(characterScale)から
// 両方を算出する形で実現した。
// ============================================================

// EnemyFactory::CreateAIController()がこれを見てAddComponentする型を
// 切り替える。新しい敵種(専用AIControllerを持つもの)を追加する場合は
// ここに値を足し、CreateAIController()のswitchにケースを足すこと。
enum class EnemyType
{
	Brute,
	Warrock,
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

	// CharacterCollisionDefaults(標準体格)からの倍率。Hurtbox半径
	// (GetHurtboxRadius())・見た目スケール(GetModelScale())・
	// (将来的な)Bodyコライダー寸法は、すべてこの1つの値から算出する
	// (クラス冒頭コメント参照)。個別に見た目だけ・当たり判定だけを
	// 変えたいケースが実際に出てきたら、その時点でこの一元化を
	// 崩すかどうかを再検討すること。
	float characterScale = 1.0f;

	// 意思決定・攻撃パターン等、AIController向けのチューニング値。
	// EnemyAIData型自体はどの敵種でも共用する「データの器」であり、
	// これを解釈するロジック(BT)側の共有は意味しない
	// (WarrockAIData.h冒頭コメント参照)。
	EnemyAIData aiData;

	// --- 体格関連の算出値 ---------------------------------------------
	// Hurtbox(被弾判定)の球半径。標準体格の横幅(kBodyWidth)の半分に
	// characterScaleを掛けて求める。
	float GetHurtboxRadius() const {
		return CharacterCollisionDefaults::kBodyWidth * 0.5f * characterScale;
	}

	// 見た目のモデルスケール。現状は等方スケールのみ想定(縦横で比率を
	// 変えたい敵種が実際に出てきたら、その時にVector3で個別に持つ形へ
	// 拡張すること)。
	Math::Vector3 GetModelScale() const {
		return Math::Vector3(characterScale, characterScale, characterScale);
	}
};