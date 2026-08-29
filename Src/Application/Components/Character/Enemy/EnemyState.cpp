#include "EnemyStatusController.h"
#include "../../../Engine/EventBus/Event/SceneEvents.h"

// --- StateWalkRight ---
void StateWalkRight::Enter(EnemyStatusController* controller) {
	controller->SetDesiredDirection({ 1.0f, 0.0f, 0.0f });
	controller->PlayAnimation("Run", true);
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
	// 左右で専用のアニメーションは無いため、StateWalkRightと同じ"Run"を
	// 再生する(モデルの向き自体はTransform側の回転で表現される想定)。
	controller->PlayAnimation("Run", true);
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

	// アニメーション未実装のためコメントアウト(PlayerState.cppの
	// StateStagger::Enterと同じ考え方)。専用の被弾/怯みアニメーションが
	// 用意できたら再生する。
	// controller->PlayAnimation("Hit", false);
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

void StateKnockback::Exit(EnemyStatusController* /*controller*/) {}

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

	// 死亡演出の再生時間(kDespawnDelay)にちょうど収まるよう、単発再生
	// (loop=false)でアニメーション速度を自動スケーリングする
	// (PlayerStatusController::PlayAnimationの同種コメント参照)。
	//controller->PlayAnimation("Death", false, kDespawnDelay);

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
		//GLOBALEVENT.Publish(Events::Scene::SceneChangeRequestEvent{ SceneType::Result });
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

// --- EnemyStateAttack ---
// (PlayerState.h側の同名クラスとの衝突回避のためリネーム。EnemyState.h
// 冒頭コメント参照)
// BT(EnemyBTController)がTryStartAttack()を呼ぶことで開始する、
// Windup→Active→Recoveryの単発攻撃State。PlayerState.cppのStateAttackと
// 同じ3フェーズ構成だが、コンボ・踏み込み移動は持たない
// (現状の敵側移動AIがX軸パトロールのみの簡易実装のため、まずは
//  その場で攻撃するだけの最小構成にしている)。
void EnemyStateAttack::Enter(EnemyStatusController* controller) {
	phase_ = Phase::Windup;
	elapsed_ = 0.0f;

	// 攻撃中は移動しない(StateParryStun::Enterと同じ考え方)。
	controller->SetDesiredDirection(Math::Vector3::Zero);

	// 攻撃全体(Windup+Active+Recovery)の秒数を目標としてアニメーション
	// 速度を自動スケーリングする(PlayerStatusController::PlayAnimationの
	// 同種コメント参照)。
	const float targetDuration = controller->GetAttackWindupDuration()
		+ controller->GetAttackActiveDuration()
		+ controller->GetAttackRecoveryDuration();
	//controller->PlayAnimation("Attack", false, targetDuration);
}

void EnemyStateAttack::Update(EnemyStatusController* controller, float deltaTime) {
	elapsed_ += deltaTime;

	switch (phase_) {
	case Phase::Windup:
		if (elapsed_ >= controller->GetAttackWindupDuration()) {
			phase_ = Phase::Active;
			elapsed_ = 0.0f;
			controller->SetWeaponHitBoxEnabled(true); // 攻撃判定が実際に発生する一瞬だけ有効化
		}
		break;

	case Phase::Active:
		if (elapsed_ >= controller->GetAttackActiveDuration()) {
			phase_ = Phase::Recovery;
			elapsed_ = 0.0f;
			controller->SetWeaponHitBoxEnabled(false); // 判定の発生窓を閉じる
		}
		break;

	case Phase::Recovery:
		if (elapsed_ >= controller->GetAttackRecoveryDuration()) {
			// StateKnockback/StateParryStunと同じく、基準点から見て
			// 今どちら側にいるかで自然に続きのパトロールへ戻す。
			// (BT側は次のTickで再度射程判定を行い、まだ射程内なら
			//  改めてTryStartAttack()を呼んで連続攻撃になる)
			const float traveled = controller->GetCurrentPosition().x - controller->GetBasePosition().x;
			if (traveled >= 0.0f) {
				controller->ChangeStateToWalkRight();
			}
			else {
				controller->ChangeStateToWalkLeft();
			}
		}
		break;
	}
}

void EnemyStateAttack::Exit(EnemyStatusController* controller) {
	// Knockback/ParryStun等に途中で割り込まれた場合でも、HitBoxが
	// 有効なまま残らないよう無条件に閉じる
	// (PlayerState.cpp::StateAttack::Exitと同じ考え方)。
	controller->SetWeaponHitBoxEnabled(false);
}