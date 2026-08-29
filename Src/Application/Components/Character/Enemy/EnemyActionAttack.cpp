#include "EnemyActionAttack.h"
#include "EnemyBTController.h"
#include "EnemyStatusController.h"

BTNodeStatus EnemyActionAttack::Tick(EnemyBTController* context, float /*deltaTime*/)
{
	EnemyStatusController* status = context->GetStatusController();
	if (status == nullptr) return BTNodeStatus::Failure;

	// まだ攻撃を開始していなければ開始を試みる。プレイヤーの現在位置を
	// 渡すことで、攻撃開始の瞬間にそちらへ振り向かせる
	// (EnemyStatusController::TryStartAttack()のコメント参照)。
	// CanAct()がfalse(被弾硬直中等)ならTryStartAttack()自体が失敗して
	// Failureを返す。
	if (!status->IsAttacking()) {
		if (!status->TryStartAttack(context->GetPlayerPositionOrSelf())) return BTNodeStatus::Failure;
	}

	// 攻撃継続中はRunning。Recovery終了でStateAttack自身が
	// ChangeStateToWalkRight/Left()を呼んでいるはずなので、次のTickでは
	// IsAttacking()==falseになり、このノードは同じフレーム内で
	// 新しい攻撃を開始しにいく(=射程内にいる限り攻撃を続ける)。
	//
	// 【TODO】攻撃後に一呼吸置きたい場合は、ここに専用のクールダウン
	// タイマーを足すか、Sequenceに「一定時間待つ」Actionを挟むこと。
	return status->IsAttacking() ? BTNodeStatus::Running : BTNodeStatus::Success;
}
