#pragma once
#include <algorithm>
#include <random>
#include "IBTNode.h"
#include "../../Enemy/EnemyAIData.h"

// 攻撃の実行部品(判断層とは独立)。
// 技の選択元(ChooseAttack/ChooseGapCloserAttack等)をコンストラクタで注入し、
// 選んだ技をWindup→Active→Recoveryの順に実行する。
//
// Tに継承は不要。以下のメンバを持っていればよい(ダックタイピング):
//   void StopMovement()
//   Math::Vector3 GetTargetPositionOrSelf() const
//   void FaceHorizontalTarget(const Math::Vector3&)
//   void FaceTargetSmoothly(const Math::Vector3&, float)
//   void StopFacingTarget()
//   void PlayAnimation(const std::string&, bool, float, bool)
//   void SetWeaponHitBoxEnabled(const std::vector<std::string>&, bool)
//   void SetWeaponTrailEmitting(const std::vector<std::string>&, bool)
//   void SetWeaponWindEmitting(const std::vector<std::string>&, bool, const std::string& preset)
//   void ApplyAttackVisualData(const std::vector<std::string>&, const AttackVisualData&)
//   void NotifyAttackCompleted(float)
//   void SetVulnerable(bool)
//   void PlayTelegraphEffect(const EnemyAttackTelegraphData&, float convergeDuration)
//   void FinishTelegraphEffect()
//   void StopTelegraphEffect()
//   void PlayAttackSwingSE()
//   const EnemyAttackDefinition* ResolveComboNext(const EnemyAttackDefinition&)
//
// 各フェーズの尺はattackData.phaseDataの値を使う。
// Windupの長さは攻撃開始時に1回だけ、windupJitterRatio(0.2なら±20%)でゆらぎを付けて決める。
// 向き: EnemyAttackDefinition::facingに従い、Windup/Activeの間だけターゲットへ滑らかに向き直る
//       (Recoveryでは追従しない)。windupTurnSpeedが0以下の技は、開始時に一度だけ瞬時に正対する。
//       追従の解除はフェーズ移行時とReset()で行う(解除し忘れると移動方向への自動追従が効かなくなる)。
// 隙: SetVulnerable(true)はRecovery中だけ。完了・中断どちらでも抜ける時にfalseへ戻す。
// インターバル: Recovery完了時にNotifyAttackCompleted()を呼ぶ。中断(Reset)時は呼ばない。
//
// IBTNode::Reset()はcontextを受け取れないため、Tick()冒頭でlastContext_にキャッシュして
// Reset()から使う(Active中の中断でHitBoxを閉じるために必要)。
// 技の形がWarrock固有に崩れてきたら、複製せずフェーズ遷移前後のフックを足す形で拡張すること。
template <typename T>
class BTWeightedAttackAction : public IBTNode<T>
{
public:
	// 技の選択元(ChooseAttack/ChooseGapCloserAttack等)
	using ChooseFn = std::function<const EnemyAttackDefinition* (T*)>;

	explicit BTWeightedAttackAction(ChooseFn chooseAttack) : chooseAttack_(std::move(chooseAttack)) {}

	BTNodeStatus Tick(T* context, float deltaTime) override
	{
		lastContext_ = context;	// Reset()の後始末用にキャッシュ

		if (phase_ == Phase::NotStarted) {
			const EnemyAttackDefinition* picked = chooseAttack_(context);
			if (picked == nullptr) return BTNodeStatus::Failure;
			StartAttack(context, picked);
		}

		elapsed_ += deltaTime;

		const AttackPhaseData& phaseData = current_->attackData.phaseData;
		switch (phase_) {
		case Phase::Windup:
			// 判定発生のleadTime前に、予備動作エフェクトを1回だけ出す
			if (!telegraphFired_ && current_->telegraph.type != TelegraphType::None &&
				elapsed_ >= windupDuration_ - current_->telegraph.leadTime) {
				telegraphFired_ = true;
				// Windup終了ちょうどに収束しきるよう、残り時間を渡す
				context->PlayTelegraphEffect(current_->telegraph, std::max(windupDuration_ - elapsed_, 0.016f));
			}
			if (elapsed_ >= windupDuration_) {
				phase_ = Phase::Active;
				elapsed_ = 0.0f;
				// 判定が発生する間だけHitBoxを有効にする(対象はAttackData::weaponSlots)
				context->FinishTelegraphEffect();
				context->ApplyAttackVisualData(current_->attackData.weaponSlots, current_->attackData.visualData);
				context->SetWeaponHitBoxEnabled(current_->attackData.weaponSlots, true);
				context->SetWeaponTrailEmitting(current_->attackData.weaponSlots, true);
				context->PlayAnimation(current_->attackData.phaseData.active);
				context->PlayAttackSwingSE();	// 振りの音は判定発生に合わせる
			}
			break;

		case Phase::Active:
			if (elapsed_ >= phaseData.active.duration) {
				phase_ = Phase::Recovery;
				elapsed_ = 0.0f;
				context->SetWeaponHitBoxEnabled(current_->attackData.weaponSlots, false);
				context->SetWeaponTrailEmitting(current_->attackData.weaponSlots, false);
				context->SetWeaponWindEmitting(current_->attackData.weaponSlots, false, "");
				context->PlayAnimation(current_->attackData.phaseData.recovery);
				context->SetVulnerable(true);	// ここから隙
			}
			break;

		case Phase::Recovery:
			if (TryChainCombo(context)) break;	// 次の段のWindupへ
			if (elapsed_ >= phaseData.recovery.duration) {
				// やり切った時だけインターバルを開始する(コンボ最終段の値を使う)
				context->NotifyAttackCompleted(current_->intervalAfter);
				Reset();
				return BTNodeStatus::Success;
			}
			break;

		default:
			break;
		}

		// 現在のフェーズに応じて向き直りを更新する(Recovery完了時は上でReset済みのためここへ来ない)
		UpdateFacing(context);

		return BTNodeStatus::Running;
	}

	void Reset() override
	{
		// Windup中断時にエフェクトを残さない
		if (telegraphFired_ && lastContext_ != nullptr) {
			lastContext_->StopTelegraphEffect();
		}
		telegraphFired_ = false;

		// Active中の中断では、HitBoxを有効なまま残さない
		if (phase_ == Phase::Active && lastContext_ != nullptr && current_ != nullptr) {
			lastContext_->SetWeaponHitBoxEnabled(current_->attackData.weaponSlots, false);
			lastContext_->SetWeaponTrailEmitting(current_->attackData.weaponSlots, false);
		}

		// 風はWindupから出すため、Windup/Active中の中断で止める
		if ((phase_ == Phase::Windup || phase_ == Phase::Active) && lastContext_ != nullptr && current_ != nullptr) {
			lastContext_->SetWeaponWindEmitting(current_->attackData.weaponSlots, false, "");
		}

		// 完了・中断のどちらでもここを通るため、隙の状態を確実に解除する
		if (phase_ == Phase::Recovery && lastContext_ != nullptr) {
			lastContext_->SetVulnerable(false);
		}

		// 向き直りの追従を必ず解除する
		if (phase_ != Phase::NotStarted && lastContext_ != nullptr) {
			lastContext_->StopFacingTarget();
		}

		phase_ = Phase::NotStarted;
		elapsed_ = 0.0f;
		current_ = nullptr;
	}

private:
	// Windup/Activeのturn速度が正なら追従し、それ以外(0以下、Recovery)は解除する
	void UpdateFacing(T* context) const
	{
		float turnSpeed = 0.0f;
		if (phase_ == Phase::Windup) turnSpeed = current_->facing.windupTurnSpeed;
		else if (phase_ == Phase::Active) turnSpeed = current_->facing.activeTurnSpeed;

		if (turnSpeed > 0.0f) {
			context->FaceTargetSmoothly(context->GetTargetPositionOrSelf(), turnSpeed);
		}
		else {
			context->StopFacingTarget();
		}
	}

	// 技の開始(最初の技・コンボの次の段で共用)。Windupを初期化する
	void StartAttack(T* context, const EnemyAttackDefinition* def)
	{
		current_ = def;
		phase_ = Phase::Windup;
		elapsed_ = 0.0f;
		telegraphFired_ = false;
		comboChecked_ = false;
		windupDuration_ = def->attackData.phaseData.windup.duration;

		context->StopMovement();
		// 追従しない技だけ、ここで一度瞬時に正対する
		if (def->facing.windupTurnSpeed <= 0.0f) {
			context->FaceHorizontalTarget(context->GetTargetPositionOrSelf());
		}
		context->PlayAnimation(def->attackData.phaseData.windup);
		context->SetWeaponWindEmitting(def->attackData.weaponSlots, true, def->attackData.visualData.windPreset);
	}

	// キャンセル開始に達したら1回だけリンクを判定し、繋がれば次の段を始める
	bool TryChainCombo(T* context)
	{
		const AttackData& data = current_->attackData;
		if (comboChecked_ || current_->comboLinks.empty()) return false;
		if (elapsed_ < std::min(data.cancelData.recoveryAttackCancelStart, data.phaseData.recovery.duration)) return false;

		comboChecked_ = true;
		const EnemyAttackDefinition* next = context->ResolveComboNext(*current_);
		if (next == nullptr) return false;

		context->SetVulnerable(false);	// 中間段の隙は閉じる
		StartAttack(context, next);
		return true;
	}

	enum class Phase { NotStarted, Windup, Active, Recovery };
	Phase phase_ = Phase::NotStarted;
	float elapsed_ = 0.0f;
	float windupDuration_ = 0.0f;	// 今回抽選したWindupの長さ
	bool telegraphFired_ = false;	// 今回予備動作を出したか
	bool comboChecked_ = false;		// 今回の段でコンボ判定を済ませたか
	const EnemyAttackDefinition* current_ = nullptr;

	T* lastContext_ = nullptr;	// Reset()用のcontextキャッシュ

	ChooseFn chooseAttack_;
};
