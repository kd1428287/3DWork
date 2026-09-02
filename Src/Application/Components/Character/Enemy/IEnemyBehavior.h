#pragma once
#include <memory>
#include "../BehaviorTree/IBTNode.h"
#include "../../Collision/AttackSourceComponent.h"

class EnemyAIController;

// ============================================================
// 敵種ごとの「判断層」+「固有の反応」をまとめたインターフェース。
//
// 【背景】以前はEnemyAIController/WarrockAIControllerという別々の
// コンポーネントを敵種ごとに用意していたが、実行層(移動・アニメーション・
// 索敵・武器制御等)がほとんど重複し、EnemyFactory側もIEnemyAIController
// 経由で武器を取り付ける迂回が必要だった。
// 今回、コンポーネント自体はEnemyAIController1種類に統合し、
// 「どう行動を選ぶか(BuildTree)」「被弾/死亡/登場時に何をするか」
// といった敵種固有の判断・反応だけをこのIEnemyBehaviorへ切り出す形に
// した。実行層はEnemyAIController側に共通実装として残る。
//
// 【AttackSourceComponentを前方宣言ではなく実体includeしている理由】
// OnParried()の引数にAttackSourceComponent::ParriedEvent(ネストされた型)を
// 名指しする必要があり、前方宣言だけではコンパイラがそのネスト名を
// 解決できないため、フル定義をincludeする形に変更した(OnHit()の
// 「const AttackSourceComponent&」のような単純な参照引数なら前方宣言
// だけで足りていたが、ネストされた型名の参照はそれでは不十分)。
//
// 【現状について】現在デバッグ表示で動かしながら詳細を詰めているのは
// Warrock(WarrockBehavior)。被弾リアクション・咆哮・死亡演出といった
// Warrock側の要件を基準にこのインターフェースの形を決めている。
// 汎用敵(BruteBehavior)側の実装は現時点でこの形に追従できていない
// 部分がある(各Behaviorのファイル冒頭コメント参照)。
// ============================================================
class IEnemyBehavior
{
public:
	virtual ~IEnemyBehavior() = default;

	// 判断層(ビヘイビアツリー)を組み立てる。EnemyAIController::Start()
	// から1回だけ呼ばれる。ownerはBTノードのTick()から実行層APIへ
	// アクセスするためのEnemyAIControllerそのもの(木の構築自体には
	// 使わないことが多いが、将来データに応じて木の構造を変えたく
	// なった場合のために渡している)。
	virtual std::unique_ptr<IBTNode<EnemyAIController>> BuildTree(EnemyAIController* owner) = 0;

	// 生成直後(Start()の終わり、BuildTree()の後)に1回だけ呼ばれる
	// フック。例: Warrockの登場時Roar要求。何もしない敵種はデフォルト
	// 実装(何もしない)のままでよい。
	virtual void OnSpawned(EnemyAIController* owner) {}

	// HurtBoxへの被弾が確定し、EnemyAIController側でHealthComponentへ
	// ダメージを適用した直後に呼ばれるフック。体幹削り・被弾リアクション
	// 要求など、敵種ごとに異なる反応はここで行う(EnemyAIController自体は
	// ダメージ適用と多段ヒット防止だけを行い、その先の解釈はしない)。
	virtual void OnHit(EnemyAIController* owner, const AttackSourceComponent& attack) {}

	// 自分自身の攻撃(そのHitBoxを持つ武器のAttackSourceComponent::
	// ownerCharacterが自分自身を指している攻撃)がパリィされた時に呼ばれる
	// フック。EnemyAIController側が自身のローカルEventBusでこのイベントを
	// Subscribe<AttackSourceComponent::ParriedEvent>()しており、受信した
	// 瞬間にこれを呼ぶ(AttackSourceComponent::ParriedEvent冒頭コメント
	// 参照)。何もしない敵種はデフォルト実装(何もしない)のままでよい。
	virtual void OnParried(EnemyAIController* owner, const AttackSourceComponent::ParriedEvent& event) {}

	// HealthComponentのDiedEventを受けてEnemyAIController側が
	// 移動停止・当たり判定無効化等の共通死亡処理を終えた直後に呼ばれる
	// フック。Dyingアニメーションの再生等、敵種固有の死亡演出はここで
	// 行う。
	virtual void OnDied(EnemyAIController* owner) {}

	// 死亡確定から消滅(RequestDespawn)までの猶予秒数。敵種ごとに死亡
	// 演出の長さが変わりうるため、Behavior側の値を使う。
	virtual float GetDespawnDelay() const { return 1.5f; }
};