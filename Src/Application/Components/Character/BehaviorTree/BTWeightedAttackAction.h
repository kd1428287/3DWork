#pragma once
#include "../BehaviorTree/IBTNode.h"
#include "../Enemy/EnemyAIData.h"

// ============================================================
// 攻撃行動の共通「実行」部品(「判断」層とは独立)。
// EnemyAIData::attacksから重み付き抽選で1つ選び、
// Windup→Active→Recoveryの3フェーズで実行する。
//
// 【この部品を作った経緯・改善案2】
// 旧EnemyActionAttack/WarrockActionAttackは、Windup/Active/Recoveryの
// ステートマシンが一文字も変わらない完全なコピーだった。
// 「Warrock固有にしたい」のはBuildTree()の構造(判断層)であって、
// 「1回の攻撃をどう実行するか」という手続き(実行層)まで別物にする
// 意図ではなかったため、実行層側だけをここへ切り出して共有する。
//
// Tには継承を要求しない(ダックタイピング)。以下のメンバ関数を
// 持ってさえいれば、EnemyAIController/WarrockAIController/将来の
// 敵種のいずれでもそのまま使い回せる:
//   const EnemyAttackDefinition* ChooseAttack() const
//   void StopMovement()
//   Math::Vector3 GetTargetPositionOrSelf() const
//   void FaceHorizontalTarget(const Math::Vector3&)
//   void PlayAnimation(const std::string&, bool, float, bool)
//   void SetWeaponHitBoxEnabled(bool)
//
// 【IBTNode::Reset()の制約について】
// IBTNode<T>::Reset()はcontext引数を受け取らない仕様のため、Active中
// (HitBoxが有効な最中)に中断された場合にSetWeaponHitBoxEnabled(false)
// で閉じたくてもcontrollerを直接参照できない。そのためTick()の冒頭で
// 直前のcontextをlastContext_へキャッシュしておき、Reset()からは
// それを使う回避策を取っている(旧EnemyActionAttack/WarrockActionAttack
// と同じ回避策)。
//
// 【今後の展望・改善案3】
// もし「攻撃の形そのもの」(例: JumpAttackだけ移動を伴う突進にする等)
// がWarrock固有に崩れてきたら、この共通化は足かせになる可能性がある。
// その場合はこのテンプレート自体をWarrock用に複製するのではなく、
// フェーズ遷移の前後にフックできるようテンプレートを拡張することを
// 検討すること。
// ============================================================
template <typename T>
class BTWeightedAttackAction : public IBTNode<T>
{
public:
	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		lastContext_ = context; // Reset()からの後始末用にキャッシュ(クラスコメント参照)

		if (phase_ == Phase::NotStarted) {
			current_ = context->ChooseAttack();
			if (current_ == nullptr) return BTNodeStatus::Failure;

			phase_ = Phase::Windup;
			elapsed_ = 0.0f;

			context->StopMovement();

			// ルートモーションで動く技(JumpAttack等)は、アニメーション側が
			// 踏み込みと同時に向きも作り込んでいる想定のため、ここで手動の
			// 正面合わせを行うと、Windup開始時点の向きとアニメーション自体が
			// 意図する向きが競合してしまう(Player側のStateAttack::Update()で
			// useRootMotionがtrueの技だけRequestStepMove()を呼ばないのと
			// 同じ考え方)。
			if (!current_->useRootMotion) {
				context->FaceHorizontalTarget(context->GetTargetPositionOrSelf());
			}

			const float totalDuration = current_->windupDuration + current_->activeDuration + current_->recoveryDuration;
			context->PlayAnimation(current_->animationName, false, totalDuration, current_->useRootMotion);
		}

		elapsed_ += deltaTime;

		switch (phase_) {
		case Phase::Windup:
			if (elapsed_ >= current_->windupDuration) {
				phase_ = Phase::Active;
				elapsed_ = 0.0f;
				context->SetWeaponHitBoxEnabled(true); // 攻撃判定が実際に発生する一瞬だけ有効化
			}
			break;

		case Phase::Active:
			if (elapsed_ >= current_->activeDuration) {
				phase_ = Phase::Recovery;
				elapsed_ = 0.0f;
				context->SetWeaponHitBoxEnabled(false); // 判定の発生窓を閉じる
			}
			break;

		case Phase::Recovery:
			if (elapsed_ >= current_->recoveryDuration) {
				Reset();
				return BTNodeStatus::Success;
			}
			break;

		default:
			break;
		}

		return BTNodeStatus::Running;
	}

	void Reset() override
	{
		// Active中(HitBoxが有効な最中)に中断された場合は、有効なまま
		// 残らないよう明示的に閉じておく(クラスコメント参照)。
		if (phase_ == Phase::Active && lastContext_ != nullptr) {
			lastContext_->SetWeaponHitBoxEnabled(false);
		}

		phase_ = Phase::NotStarted;
		elapsed_ = 0.0f;
		current_ = nullptr;
	}

private:
	enum class Phase { NotStarted, Windup, Active, Recovery };
	Phase phase_ = Phase::NotStarted;
	float elapsed_ = 0.0f;
	const EnemyAttackDefinition* current_ = nullptr;

	// Reset()がcontextを受け取れない制約への回避策(クラス冒頭コメント参照)。
	T* lastContext_ = nullptr;
};