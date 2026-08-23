#include "EnemyStatusController.h"

// --- StateWalkRight ---
void StateWalkRight::Enter(EnemyStatusController* controller) {
	controller->SetDesiredDirection({ 1.0f, 0.0f, 0.0f });
}

void StateWalkRight::Update(EnemyStatusController* controller, float /*deltaTime*/) {
	const float traveled = controller->GetCurrentPosition().x - controller->GetBasePosition().x;
	if (traveled >= controller->GetPatrolDistance()) {
		controller->ChangeStateToWalkLeft();
	}
}

// --- StateWalkLeft ---
void StateWalkLeft::Enter(EnemyStatusController* controller) {
	controller->SetDesiredDirection({ -1.0f, 0.0f, 0.0f });
}

void StateWalkLeft::Update(EnemyStatusController* controller, float /*deltaTime*/) {
	const float traveled = controller->GetBasePosition().x - controller->GetCurrentPosition().x;
	if (traveled >= controller->GetPatrolDistance()) {
		controller->ChangeStateToWalkRight();
	}
}

// --- StateKnockback ---
// 吹っ飛ばし自体はVelocityComponent::AddImpulse()に一度だけ委譲する
// (毎フレーム位置を上書きするのではなく、物理的な減衰にそのまま任せる。
//  理由はEnemyState.hのKnockbackParamsコメント参照)。
// このStateが担うのは「いつパトロールへ復帰してよいか」の判定だけ。
void StateKnockback::Enter(EnemyStatusController* controller) {
	elapsed_ = 0.0f;
	controller->ApplyKnockbackImpulse(params_.direction * params_.power);
}

void StateKnockback::Update(EnemyStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;

	// 最低保証時間が経過していない間は、物理的に止まっていても
	// まだ怯み演出を継続する(演出上不自然に短い怯みを防ぐ)。
	if (elapsed_ < params_.minStunDuration) return;

	// 物理的にまだ吹っ飛び中(VelocityComponentの摩擦減衰がまだ閾値を
	// 上回っている)なら、パトロールへ戻さずそのまま待つ。ここで
	// SetDesiredDirection等による位置の手動書き換えは一切行わない
	// (VelocityComponent::Update()が毎フレーム自動で位置を進める)。
	if (controller->IsKnockbackImpulseActive()) return;

	// 怯み終了。基準点から見て今どちら側にいるかで、自然に続きの
	// パトロールへ戻す(元々居た側の反対へ歩き出すと不自然なため)。
	const float traveled = controller->GetCurrentPosition().x - controller->GetBasePosition().x;
	if (traveled >= 0.0f) {
		controller->ChangeStateToWalkRight();
	}
	else {
		controller->ChangeStateToWalkLeft();
	}
}

void StateKnockback::Exit(EnemyStatusController* /*controller*/) {
}

// --- StateDead ---
void StateDead::Enter(EnemyStatusController* controller) {
	elapsed_ = 0.0f;
	despawnRequested_ = false;

	// 移動・当たり判定を止める。当たり判定を無効化することで、以降
	// CollisionSystemがこのGameObjectをペア判定の対象から除外し
	// (IsCollidable参照)、OnCollisionEnterが呼ばれなくなる。これにより、
	// 死亡後に追加でダメージ/ノックバックを受けることも自然に無くなる。
	controller->StopMovementForDeath();
	controller->DisableCollisionForDeath();

	// TODO: 死亡アニメーションを再生する(ModelAnimatorComponent導入後)。
	// controller->PlayDeathAnimation();

	// TODO: 死亡エフェクト(パーティクル等)を再生する(エフェクトシステム実装後)。
	// controller->PlayDeathEffect();
}

void StateDead::Update(EnemyStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;

	if (despawnRequested_) return;

	// 死亡演出(アニメーション+エフェクト)の再生時間分だけ猶予を置いてから
	// 消滅させる。演出が未実装の間は、単なる「一定時間待ってから消える」
	// 動作になる。
	if (elapsed_ >= kDespawnDelay) {
		despawnRequested_ = true;
		controller->RequestDespawn();
	}
}

// --- StateParryStun ---
void StateParryStun::Enter(EnemyStatusController* controller) {
	elapsed_ = 0.0f;

	// その場で動きを止める。desiredDirection_もゼロにしておかないと、
	// MovementComponent再開時に古い移動方向が残ってしまう
	// (SetMovementEnabled(false)はMovementComponent自体を無効化するだけで、
	//  IMovementSourceが返す値までは書き換えないため)。
	controller->SetMovementEnabled(false);
	controller->SetDesiredDirection(Math::Vector3::Zero);

	// TODO: 武器が弾かれる演出(振り返り/仰け反りモーション、武器を
	// 弾き飛ばす物理挙動)は武器オブジェクト実装後にここへ追加する。
	// 現状は敵本体が攻撃判定を持つ仮実装のため、演出を持たせる対象
	// (弾かれるべき武器)自体がまだ存在しない。
}

void StateParryStun::Update(EnemyStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;
	if (elapsed_ < kParryStunDuration) return;

	// 怯み終了。StateKnockback::Updateと同様、基準点から見て今どちら側に
	// いるかで自然に続きのパトロールへ戻す。
	const float traveled = controller->GetCurrentPosition().x - controller->GetBasePosition().x;
	if (traveled >= 0.0f) {
		controller->ChangeStateToWalkRight();
	}
	else {
		controller->ChangeStateToWalkLeft();
	}
}

void StateParryStun::Exit(EnemyStatusController* controller) {
	controller->SetMovementEnabled(true);
}
