#include "PlayerMovementAnimationComponent.h"
#include <algorithm> // std::min

void PlayerMovementAnimationComponent::Start()
{
	modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
	transform_ = GetOwner()->GetComponent<TransformComponent>();
}

void PlayerMovementAnimationComponent::Tick(float deltaTime, MovementState state, const Math::Vector3& inputDirection, bool isLockedOn)
{
	// Turning/Start/Endの経過時間をまず進める。完了していれば内部で
	// 次のフェーズ(Start→Loop、End→Idle、Turning→Start)へ自動的に遷移する。
	AdvancePhaseTiming(deltaTime);

	const bool stateChanged = (lastState_ != state);
	const bool lockChanged = (lastLockedOn_ != isLockedOn);
	const bool wasStand = (lastState_ == MovementState::Stand);

	// Turning中は、入力がStandへ戻った場合だけ打ち切る。それ以外の方向変更は
	// ターンが完了するまで無視する(完了後にAdvancePhaseTiming経由で
	// BeginStart()が呼ばれ、以降は通常分岐へフォールスルーする)。
	if (phase_ == Phase::Turning) {
		if (state == MovementState::Stand) {
			phase_ = Phase::Loop;
			Play(kIdleAnimation, true);
		}
		lastState_ = state;
		lastLockedOn_ = isLockedOn;
		return;
	}

	lastState_ = state;
	lastLockedOn_ = isLockedOn;

	if (state == MovementState::Stand) {
		if (stateChanged) {
			BeginEnd();
		}
		// Standでの向き制御はPlayerStatusController::UpdateLockOnFacing側の管轄。
		return;
	}

	Math::Vector3 dir = inputDirection;
	dir.y = 0.0f;
	const bool hasDir = dir.LengthSquared() > kDirectionEpsilon;
	if (hasDir) dir.Normalize();

	if (state == MovementState::Run) {
		//if (stateChanged && wasStand) {
		//	BeginTurnOrStart(state, dir, /*allowTurn=*/!isLockedOn && hasDir);
		//	return;
		//}

		if(hasDir)FaceDirection(dir);

		if (stateChanged) {
			SwitchLoopRun();
		}
		return;
	}

	// --- ここからWalk ---
	if (isLockedOn) {
		// ロック中は向きを変えない(Controller::UpdateLockOnFacing側が
		// 毎フレームロック対象へ固定する)。ここでは現在の向き(transform_の
		// 前方 = ロック対象方向)に対する入力方向で8方向を判定するだけ。
		const Direction8 dir8 = (transform_ != nullptr)
			? ClassifyDirection8(transform_->GetForward(), dir)
			: Direction8::Forward;

		if ((stateChanged && wasStand) || stateChanged || lockChanged || dir8 != lockedDirection_) {
			// ロックWalkはStart/Endを持たないため、いずれの遷移でも
			// 直接Loopクリップへ切り替えるだけでよい。
			SwitchLoopWalkLocked(dir8);
		}
	}
	else {
		//if (stateChanged && wasStand) {
		//	BeginTurnOrStart(state, dir, /*allowTurn=*/hasDir);
		//	return;
		//}

		if (hasDir)FaceDirection(dir);

		if (stateChanged || lockChanged) {
			SwitchLoopWalkForward();
		}
	}
}

void PlayerMovementAnimationComponent::Refresh(MovementState state, const Math::Vector3& inputDirection, bool isLockedOn)
{
	// 【簡略化】戦闘行動からの復帰直後は「既に助走がついている」とみなし、
	// Turn/Startは挟まずLoopへ直接入る(Standへ戻る場合もEndを挟まず直接
	// Idleにする)。詳細は元のRefreshMovementAnimationのコメントと同じ
	// 【今後の検討事項】。
	lastState_ = state;
	lastLockedOn_ = isLockedOn;
	phase_ = Phase::Loop;
	elapsed_ = 0.0f;

	if (state == MovementState::Stand) {
		Play(kIdleAnimation, true);
		return;
	}

	Math::Vector3 dir = inputDirection;
	dir.y = 0.0f;
	const bool hasDir = dir.LengthSquared() > kDirectionEpsilon;
	if (hasDir) dir.Normalize();

	if (state == MovementState::Run) {
		if (hasDir) FaceDirection(dir);
		SwitchLoopRun();
		return;
	}

	// Walk
	if (isLockedOn) {
		const Direction8 dir8 = (transform_ != nullptr) ? ClassifyDirection8(transform_->GetForward(), dir) : Direction8::Forward;
		SwitchLoopWalkLocked(dir8);
	}
	else {
		if (hasDir) FaceDirection(dir);
		SwitchLoopWalkForward();
	}
}

void PlayerMovementAnimationComponent::AdvancePhaseTiming(float deltaTime)
{
	if (phase_ == Phase::Turning) {
		UpdateTurning(deltaTime);
		return;
	}
	if (phase_ != Phase::Start && phase_ != Phase::End) return;

	elapsed_ += deltaTime;
	const float duration = (phase_ == Phase::Start) ? CurrentStartDuration() : CurrentEndDuration();
	if (elapsed_ < duration) return;

	if (phase_ == Phase::Start) {
		CompleteStartToLoop();
	}
	else {
		phase_ = Phase::Loop;
		Play(kIdleAnimation, true);
	}
}

void PlayerMovementAnimationComponent::UpdateTurning(float deltaTime)
{
	elapsed_ += deltaTime;

	// 回転の適用はもうここの責務ではない。RootMotionApplierComponentが
	// modelAnimatorComponent_->ConsumeRootMotionYawDelta()を毎フレーム
	// 消費してtransform_へ反映している。ここでは「ターンが終わったか」
	// という時間経過だけを見る。
	if (elapsed_ >= turnDuration_) {
		// Begin*系が「状態変化」と誤検知してターンをやり直さないよう、
		// ここで先にlastState_を確定させてからBeginStart()を呼ぶ。
		lastState_ = pendingStateAfterTurn_;
		BeginStart(pendingStateAfterTurn_);
	}
}

void PlayerMovementAnimationComponent::BeginTurnOrStart(MovementState state, const Math::Vector3& horizontalDir, bool allowTurn)
{
	if (horizontalDir.LengthSquared() <= kDirectionEpsilon || transform_ == nullptr) {
		BeginStart(state);
		return;
	}

	const TurnDirection turn = allowTurn
		? ClassifyTurnDirection(transform_->GetForward(), horizontalDir)
		: TurnDirection::None;

	if (turn == TurnDirection::None) {
		FaceDirection(horizontalDir);
		BeginStart(state);
		return;
	}

	const ActionPhaseData& turnData = ResolveTurnData(turn);

	pendingStateAfterTurn_ = state;
	turnDuration_ = turnData.duration;
	elapsed_ = 0.0f;
	phase_ = Phase::Turning;

	// 回転はもう自前計算しない。ターンクリップに焼き込まれた回転を
	// RootMotionExtractor/RootMotionApplierComponentが毎フレーム抽出・適用する。
	Play(turnData.animationName, false, turnData.duration, turnData.useRootMotion, /*extractRotation=*/true);
}

void PlayerMovementAnimationComponent::BeginStart(MovementState state)
{
	if (state == MovementState::Run) {
		loopSource_ = LoopSource::Run;
		if (!runAnimSet_.startAnimationName.empty()) {
			phase_ = Phase::Start;
			elapsed_ = 0.0f;
			Play(runAnimSet_.startAnimationName, false, runAnimSet_.startDuration);
		}
		else {
			SwitchLoopRun();
		}
	}
	else {
		loopSource_ = LoopSource::WalkForward;
		const MovementPhaseClips& clips = walkAnimSet_.forward;
		if (!clips.startAnimationName.empty()) {
			phase_ = Phase::Start;
			elapsed_ = 0.0f;
			Play(clips.startAnimationName, false, clips.startDuration);
		}
		else {
			SwitchLoopWalkForward();
		}
	}
}

void PlayerMovementAnimationComponent::SwitchLoopWalkForward()
{
	loopSource_ = LoopSource::WalkForward;
	phase_ = Phase::Loop;
	Play(walkAnimSet_.forward.loopAnimationName, true);
}

void PlayerMovementAnimationComponent::SwitchLoopWalkLocked(Direction8 dir)
{
	loopSource_ = LoopSource::WalkLocked;
	lockedDirection_ = dir;
	phase_ = Phase::Loop;
	Play(walkAnimSet_.locked.Get(dir), true);
}

void PlayerMovementAnimationComponent::SwitchLoopRun()
{
	loopSource_ = LoopSource::Run;
	phase_ = Phase::Loop;
	Play(runAnimSet_.loopAnimationName, true);
}

void PlayerMovementAnimationComponent::BeginEnd()
{
	// ロックWalk(8方向)はStart/Endを持たないため、その場合は即座にIdleへ。
	if (loopSource_ == LoopSource::WalkLocked) {
		phase_ = Phase::Loop;
		Play(kIdleAnimation, true);
		return;
	}

	const MovementPhaseClips& clips = (loopSource_ == LoopSource::Run) ? runAnimSet_ : walkAnimSet_.forward;
	if (!clips.endAnimationName.empty()) {
		phase_ = Phase::End;
		elapsed_ = 0.0f;
		Play(clips.endAnimationName, false, clips.endDuration);
	}
	else {
		phase_ = Phase::Loop;
		Play(kIdleAnimation, true);
	}
}

void PlayerMovementAnimationComponent::CompleteStartToLoop()
{
	if (loopSource_ == LoopSource::Run) SwitchLoopRun();
	else SwitchLoopWalkForward();
}

float PlayerMovementAnimationComponent::CurrentStartDuration() const
{
	return (loopSource_ == LoopSource::Run) ? runAnimSet_.startDuration : walkAnimSet_.forward.startDuration;
}

float PlayerMovementAnimationComponent::CurrentEndDuration() const
{
	return (loopSource_ == LoopSource::Run) ? runAnimSet_.endDuration : walkAnimSet_.forward.endDuration;
}

const ActionPhaseData& PlayerMovementAnimationComponent::ResolveTurnData(TurnDirection turn) const
{
	switch (turn) {
	case TurnDirection::Left90:   return turnAnimSet_.left90;
	case TurnDirection::Right90:  return turnAnimSet_.right90;
	case TurnDirection::Left180:  return turnAnimSet_.left180;
	case TurnDirection::Right180:
	default:                      return turnAnimSet_.right180;
	}
}

void PlayerMovementAnimationComponent::FaceDirection(const Math::Vector3& horizontalDir)
{
	if (transform_ == nullptr) return;
	if (horizontalDir.LengthSquared() <= kDirectionEpsilon) return;

	const float yaw = ComputeHorizontalAngleTo(Math::Vector3::Forward, horizontalDir);
	transform_->SetRotation(Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw));
}

void PlayerMovementAnimationComponent::Play(const std::string& name, bool loop,
	float targetDurationSeconds, bool useRootMotion, bool extractRotation)
{
	if (modelAnimatorComponent_ == nullptr) return;

	modelAnimatorComponent_->SetRootMotionBoneName(useRootMotion ? kRootMotionBoneName : "");
	modelAnimatorComponent_->SetRootMotionExtractRotation(extractRotation); // 追加
	modelAnimatorComponent_->SetBlendDuration(kBlendDuration);
	modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
}