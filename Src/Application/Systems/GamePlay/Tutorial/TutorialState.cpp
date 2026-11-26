#include "TutorialState.h"
#include "Application/Factories/GamePlay/Character/EnemySpawner.h"
#include "Application/Core/EventBus/Events/CombatEvents.h"
#include "Application/Components/GamePlay/Character/Enemy/EnemyAIController.h"

// --- ReachArea State ---
void StateReachArea::Enter(TutorialContext* ctx) {
	isReached_ = false;
	// TODO: 説明UIと目標マーカーを表示する
}

void StateReachArea::Update(TutorialContext* ctx, float deltaTime) {
	// TODO: プレイヤーが目標の範囲内に入ったらisReached_をtrueにする
}

void StateReachArea::Exit(TutorialContext* ctx) {
	// TODO: 説明UIと目標マーカーを消す
}

// --- DefeatEnemies State ---
void StateDefeatEnemies::Enter(TutorialContext* ctx) {
	isCleared_ = false;
	// TODO: 説明UIを表示する
	for (const EnemySpawnData& spawn : data_.enemies) {
		enemies_.push_back(ctx->enemySpawner->Spawn(spawn.prefabName, spawn.position));
	}
}

void StateDefeatEnemies::Update(TutorialContext* ctx, float deltaTime)
{
	if (isCleared_) return;

	// 死亡・破棄された敵を除去し、空になれば全滅
	std::erase_if(enemies_, [](const Handle<GameObject>& e) { return IsEnemyDead(e); });
	isCleared_ = enemies_.empty();
}

void StateDefeatEnemies::Exit(TutorialContext* ctx) {
	// TODO: 説明UIとロックオンを片付ける
	for (Handle<GameObject> enemy : enemies_) ctx->enemySpawner->Despawn(enemy);
	enemies_.clear();
}

bool StateDefeatEnemies::IsEnemyDead(const Handle<GameObject>& enemy)
{
	if (!enemy.IsValid()) return true;
	return enemy.Resolve()->GetComponent<EnemyAIController>()->IsDead();
}

// --- Parry State ---
void StateParry::Enter(TutorialContext* ctx) {
	phase_ = Phase::WaitAttack;
	successCount_ = 0;
	isCompleted_ = false;
	enemy_ = ctx->enemySpawner->Spawn(data_.enemy.prefabName, data_.enemy.position);
	// TODO: 説明UIを表示する
	// TODO: HitReaction(Parry)を購読し、NotifyParrySuccess()を呼ぶ
}

void StateParry::Update(TutorialContext* ctx, float deltaTime) {
	switch (phase_) {
	case Phase::WaitAttack: UpdateWaitAttack(ctx); break;
	case Phase::Prompt:     UpdatePrompt(ctx); break;
	case Phase::Resolve:    UpdateResolve(ctx); break;
	}
}

void StateParry::Exit(TutorialContext* ctx) {
	// TODO: 時間速度を戻し、購読解除、UIとロックオンを片付ける
	ctx->enemySpawner->Despawn(enemy_);
	enemy_ = Handle<GameObject>();
}

void StateParry::UpdateWaitAttack(TutorialContext* ctx) {
	// TODO: 着弾直前の通知を受けたら、プレイヤーをNoneへ戻して時間停止し、Promptへ
	// TODO: data_.pauseOnPromptがtrueの時だけ停止し、初回(または失敗が続いた時)に限る
}

void StateParry::UpdatePrompt(TutorialContext* ctx) {
	// TODO: IsInParryWindow()がtrueになったら時間を再開し、UIを消してResolveへ
}

void StateParry::UpdateResolve(TutorialContext* ctx) {
	// TODO: 敵の攻撃の成否が確定したらWaitAttackへ戻る
}

void StateParry::NotifyParrySuccess() {
	++successCount_;
	isCompleted_ = successCount_ >= data_.requiredCount;
}
