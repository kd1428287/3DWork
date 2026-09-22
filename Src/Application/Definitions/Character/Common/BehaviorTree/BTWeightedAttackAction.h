#pragma once
#include "IBTNode.h"
#include "../../Enemy/EnemyAIData.h"

// ============================================================
// 攻撃行動の共通「実行」部品(「判断」層とは独立)。
// どのプールから選ぶか(ChooseAttack/ChooseGapCloserAttack)はコンストラクタで
// 注入し、重み付き抽選で1つ選んでWindup→Active→Recoveryで実行する。
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
//   void StopMovement()
//   Math::Vector3 GetTargetPositionOrSelf() const
//   void FaceHorizontalTarget(const Math::Vector3&)
//   void PlayAnimation(const std::string&, bool, float, bool)
//   void SetWeaponHitBoxEnabled(const std::vector<std::string>&, bool)
//   void SetWeaponTrailEmitting(const std::vector<std::string>&, bool)
//   void NotifyAttackCompleted()
//
// 【IBTNode::Reset()の制約について】
// IBTNode<T>::Reset()はcontext引数を受け取らない仕様のため、Active中
// (HitBoxが有効な最中)に中断された場合にSetWeaponHitBoxEnabled(false)
// で閉じたくてもcontrollerを直接参照できない。そのためTick()の冒頭で
// 直前のcontextをlastContext_へキャッシュしておき、Reset()からは
// それを使う回避策を取っている(旧EnemyActionAttack/WarrockActionAttack
// と同じ回避策)。
//
// 【攻撃インターバルについて】
// Recoveryフェーズが完了しSuccessを返す直前にcontext->NotifyAttackCompleted()
// を呼び、次の攻撃までのインターバル(EnemyAIData::attackIntervalDuration)を
// 開始させる。中断(Reset()経由)された場合はこの通知を行わない
// (攻撃をやり切っていない以上、インターバルを課す理由が無いため)。
//
// 【フェーズ遷移が実データを読むようになった経緯(修正)】
// 以前はWindup/Active/Recoveryの遷移条件が全て「elapsed_ >= 0」という
// ハードコードのままだった(EnemyAttackDefinition::attackData.phaseData
// 自体はWarrockAIData.h側で実際の秒数が設定済みだったが、この
// Tick()側がまだそれを読んでいなかった)。elapsed_は0からdeltaTime分
// しか進んでいない最初のTickで既に「0以上」を満たしてしまうため、
// Windup→Active→Recoveryが実質同じフレームで連鎖的に完了し、
// 「HitBoxが有効化された直後に無効化される」「アニメーションが
// 最後まで再生される前に攻撃自体が終わる」といった不具合の原因に
// なっていた。実際にphaseData.windup/active/recovery.targetDurationを
// 読むよう修正した。
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
	// どのプールから選ぶか(ChooseAttack/ChooseGapCloserAttack等)を
	// 呼び出し側が注入する。
	using ChooseFn = std::function<const EnemyAttackDefinition* (T*)>;

	explicit BTWeightedAttackAction(ChooseFn chooseAttack) : chooseAttack_(std::move(chooseAttack)) {}

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		lastContext_ = context; // Reset()からの後始末用にキャッシュ(クラスコメント参照)

		if (phase_ == Phase::NotStarted) {
			current_ = chooseAttack_(context);
			if (current_ == nullptr) return BTNodeStatus::Failure;

			phase_ = Phase::Windup;
			elapsed_ = 0.0f;

			context->StopMovement();
			context->FaceHorizontalTarget(context->GetTargetPositionOrSelf());
			context->PlayAnimation(current_->attackData.phaseData.windup);
		}

		elapsed_ += deltaTime;

		const AttackPhaseData& phaseData = current_->attackData.phaseData;
		switch (phase_) {
		case Phase::Windup:
			if (elapsed_ >= phaseData.windup.duration) {
				phase_ = Phase::Active;
				elapsed_ = 0.0f;
				// 攻撃判定が実際に発生する一瞬だけ有効化(Player同様
				// AttackData::weaponSlotsで対象スロットを指定する。
				// BTWeightedAttackAction冒頭コメントのダックタイピング
				// 一覧を参照)。
				context->SetWeaponHitBoxEnabled(current_->attackData.weaponSlots, true);
				context->SetWeaponTrailEmitting(current_->attackData.weaponSlots, true);
				context->PlayAnimation(current_->attackData.phaseData.active);
			}
			break;

		case Phase::Active:
			if (elapsed_ >= phaseData.active.duration) {
				phase_ = Phase::Recovery;
				elapsed_ = 0.0f;
				context->SetWeaponHitBoxEnabled(current_->attackData.weaponSlots, false); // 判定の発生窓を閉じる
				context->SetWeaponTrailEmitting(current_->attackData.weaponSlots, false);
				context->PlayAnimation(current_->attackData.phaseData.recovery);
			}
			break;

		case Phase::Recovery:
			if (elapsed_ >= phaseData.recovery.duration) {
				// 攻撃1回分をやり切った時だけ、次の攻撃までのインターバルを
				// 開始させる(クラス冒頭コメント参照)。
				context->NotifyAttackCompleted();
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
		if (phase_ == Phase::Active && lastContext_ != nullptr && current_ != nullptr) {
			lastContext_->SetWeaponHitBoxEnabled(current_->attackData.weaponSlots, false);
			lastContext_->SetWeaponTrailEmitting(current_->attackData.weaponSlots, false);
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

	ChooseFn chooseAttack_;
};
