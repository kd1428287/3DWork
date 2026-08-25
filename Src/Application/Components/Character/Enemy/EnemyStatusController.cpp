#include "EnemyStatusController.h"

// --- 当たり判定連携 ---------------------------------------------------

void EnemyStatusController::OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e) {
	// このコントローラが反応すべきは自分側がHurtBoxとして受けた通知だけ。
	// (同じGameObjectが足元の接地レイ用コライダー等、他の形状を
	//  持っている場合に、それらのEnterと混ざらないようにするため)
	if (e.selfShapeName != "HurtBox") return;

	AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
	if (attack == nullptr) return;

	// 多段ヒット防止。同じ攻撃(=HitBoxがenabled=trueになっている間)で
	// 既にこの相手(自分)へヒット済みなら無視する。HitBoxが再度
	// enabled=trueになる(=次の攻撃が始まる)たびにPlayerStatusController::
	// SetWeaponHitBoxEnabled()側でクリアされる想定(alreadyHitの
	// クリアタイミングは攻撃側が管理する。詳細はAttackSourceComponent.h参照)。
	if (attack->alreadyHit.count(GetOwner()) > 0) return;
	attack->alreadyHit.insert(GetOwner());

	// EnemyStatusControllerにはガード/パリィの概念が無いため、
	// 防御の有無に関わらず「攻撃を受けただけで体幹が溜まる」という
	// Sekiro寄りの仕様にしている(体幹が最大まで溜まった後の
	// 崩し状態への遷移はまだ未実装。PostureComponent::IsBroken()参照)。
	if (healthComponent_ != nullptr) {
		healthComponent_->TakeDamage(attack->damage);
	}
	if (postureComponent_ != nullptr) {
		postureComponent_->AddPostureDamage(attack->postureDamage);
		if (postureComponent_->IsBroken()) {
			// TODO: 崩し状態(専用State)への遷移は別途実装。
		}
	}

	// ノックバック方向は、攻撃判定を持つ武器自体(e.otherObject)の位置
	// ではなく、武器の持ち主本体(AttackSourceComponent::ownerCharacter)
	// の位置を基準にする。武器はBoneSocketComponent経由で手のボーンに
	// 追従しており、攻撃モーション中は絶えず振り回されて位置が動く。
	// 武器自体の位置を基準にすると、振りの軌道次第で武器が一瞬
	// 「敵から見てプレイヤーの向こう側」に来ることがあり、その瞬間の
	// 位置関係でノックバック方向を計算すると、プレイヤーの方向へ
	// 向かって吹っ飛ぶという直感に反する結果になる(実際に発生していた
	// 不具合)。ownerCharacterが未設定/破棄済みの場合のみ、
	// フォールバックとして従来通り武器自体の位置を使う。
	//
	// ※現状のパトロールAI(StateWalkRight/Left)はX軸移動のみのテスト用
	//   簡易実装だが、これは移動システム側が最終的に3D対応する前提の
	//   暫定であり、当たり判定・ノックバック側をX軸限定に合わせて
	//   歪める必要は無い。
	GameObject* attackOrigin = attack->ownerCharacter.Resolve();
	if (attackOrigin == nullptr) {
		attackOrigin = e.otherObject;
	}

	Math::Vector3 dir = Math::Vector3::Forward;
	if (TransformComponent* originTransform = attackOrigin->GetComponent<TransformComponent>()) {
		dir = transform_->GetPosition() - originTransform->GetPosition();
		dir.y = 0.0f;
		if (dir.LengthSquared() < 1e-6f) {
			dir = Math::Vector3::Forward; // 真上/真下から等、水平差が無い場合のフォールバック
		}
		else {
			dir.Normalize();
		}
	}

	KnockbackParams params;
	params.direction = dir;
	params.power = attack->knockbackPower;
	params.minStunDuration = attack->hitStunSeconds;

	ChangeStateToKnockback(params);
}

// HealthComponent::DiedEvent受信時に呼ばれる。以降のパトロール/
// ノックバック処理を打ち切り、StateDeadへ強制遷移する
// (ForceTransitionToなので、たとえ既にStateDead中でも安全に呼べる。
//  ただしHealthComponent側の多重発行防止により、実際にはこの
//  ハンドラ自体が2回呼ばれることは無い想定)。
void EnemyStatusController::OnDied(const HealthComponent::DiedEvent& /*e*/) {
	stateMachine_.ForceTransitionTo(this, &stateDead_);
}

// --- ノックバックの安全域クランプ(地面すり抜け対策) -----------------------

KnockbackParams EnemyStatusController::ClampKnockbackParams(KnockbackParams params) {
	params.direction = ClampKnockbackDirection(params.direction);
	params.power = std::min(params.power, data_.maxKnockbackSpeed);
	return params;
}

// direction.yを単純に上書きしてからNormalize()すると、水平成分が
// 小さいベクトル(ほぼ真下/真上向き)ほど正規化の過程でY成分が元の
// 値近くまで引き戻されてしまい、クランプが実質無効化される
// (水平成分以外に正規化がすがる先が無いため)。
// 例: {0.1, -0.99, 0.1}のyを-0.5に置き換えて正規化すると、
//     長さを1に戻す過程でyは-0.96近くまで戻ってしまう。
//
// 正しくは「yをクランプした上で、水平成分の長さがsqrt(1-y^2)になる
// よう水平方向だけ再スケールする」必要がある。これなら仕上がりが
// 必ず単位ベクトルになり、yも狙った値のまま保たれる。
Math::Vector3 EnemyStatusController::ClampKnockbackDirection(const Math::Vector3& direction) {
	const float clampedY = std::max(direction.y, data_.minKnockbackDirectionY);

	Math::Vector3 horizontal(direction.x, 0.0f, direction.z);
	const float horizontalLenSq = horizontal.LengthSquared();

	constexpr float kEpsilon = 1e-4f;
	if (horizontalLenSq < kEpsilon) {
		// 水平成分がほぼゼロ(真上/真下からほぼ垂直に当たったケース)。
		// 向きを再スケールする基準が無いため、既定の水平方向
		// (Forward軸)にフォールバックする。
		horizontal = Math::Vector3::Forward;
	}
	else {
		horizontal /= std::sqrt(horizontalLenSq); // 水平方向だけ正規化
	}

	// clampedY^2 + horizontalLen^2 = 1 になるよう水平成分の長さを
	// 再スケールする。これで仕上がりが必ず単位ベクトルになる。
	const float horizontalTargetLen = std::sqrt(std::max(0.0f, 1.0f - clampedY * clampedY));
	horizontal *= horizontalTargetLen;

	return Math::Vector3(horizontal.x, clampedY, horizontal.z);
}

// --- 簡易的な攻撃タイマー -------------------------------------------------
// 基底クラスのデフォルト実装。Boss等で完全に異なる攻撃パターンが
// 必要になったらoverrideすること(EnemyStatusController.hのコメント参照)。

void EnemyStatusController::UpdateAttackTimer(float deltaTime) {
	if (!CanAttack()) {
		// ノックバック/死亡/パリィ怯み中は攻撃不可。発生中のHitBoxが
		// あれば安全のため閉じ、タイマーもリセットしておく
		// (再開後に「間隔を無視していきなり攻撃が出る」ことを防ぐため)。
		if (hitBoxActiveTimer_ > 0.0f) {
			hitBoxActiveTimer_ = 0.0f;
			SetWeaponHitBoxEnabled(false);
		}
		attackIntervalTimer_ = 0.0f;
		return;
	}

	if (hitBoxActiveTimer_ > 0.0f) {
		hitBoxActiveTimer_ -= deltaTime;
		if (hitBoxActiveTimer_ <= 0.0f) {
			SetWeaponHitBoxEnabled(false);
		}
		return; // 発生中は次の間隔カウントを進めない
	}

	attackIntervalTimer_ += deltaTime;
	if (attackIntervalTimer_ < data_.attackInterval) return;

	attackIntervalTimer_ = 0.0f;
	hitBoxActiveTimer_ = data_.attackActiveDuration;
	SetWeaponHitBoxEnabled(true);
}

void EnemyStatusController::SetWeaponHitBoxEnabled(bool enabled) {
	if (ColliderComponent* collider = weaponCollider_.Resolve()) {
		collider->SetShapeEnabled("HitBox", enabled);
	}

	if (enabled) {
		// 新しい攻撃の開始として、前回までのヒット記録をクリアする
		// (PlayerStatusController::SetWeaponHitBoxEnabled()と同じ理由)。
		if (AttackSourceComponent* source = weaponAttackSource_.Resolve()) {
			source->alreadyHit.clear();
		}
	}
}