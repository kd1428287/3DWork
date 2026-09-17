#pragma once
#include "Application/Definitions/Character/Common/CharacterDefinitionCommon.h"
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "Application/Definitions/Character/Player/PlayerCombatBehaviorDefinition.h" // PlayerMovementAnimationDefinition
#include "../../../Graphics/Animation/ModelAnimatorComponent.h"

// ============================================================
// PlayerMovementAnimationComponent
// Walk/Runの向き制御とアニメーション再生を専任で担当する、
// PlayerStatusControllerの兄弟コンポーネント。
//
// 【要件まとめ】
// - Walk(歩行):
//   - 非ロックオン中: 入力方向へ向き直しながら前進する(常に「前方」1種類の
//     Start/Loop/Endクリップを使う)。
//   - ロックオン中: 向きは固定(PlayerStatusController::UpdateLockOnFacing
//     側が毎フレーム固定)。固定された向きに対する入力方向で8方向の
//     Loopクリップを切り替える(Start/Endは無し)。
// - Run(走行): 前方1方向のみ。ロック有無に関わらず常に入力方向へ正対する。
//   Start/Loop/Endの3フェーズを持つ。
// - ターン: 非ロックオン中、Standから動き出す瞬間に大きな方向転換が必要な
//   場合、90度/180度の専用ターンアニメーションを再生してから歩行/走行を
//   開始する。
//
// PlayerStatusController::HandleMovementInputからCombatStateがNoneの間
// だけ毎フレームTick()が呼ばれる想定(それ以外の間は単に呼ばれないことで、
// 全フェーズが自然に凍結される)。
// ============================================================
class PlayerMovementAnimationComponent : public ComponentBase
{
public:
	explicit PlayerMovementAnimationComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;

	// PlayerFactory構築時に、PlayerDefinition::movementAnimationsを
	// 注入するためのセッター(SetWeapon/SetAttackTableと同じ位置づけ。
	// AddComponentの戻り値に対してその場で呼ぶ想定)。
	void SetMovementAnimations(const PlayerMovementAnimationDefinition& def)
	{
		walkAnimSet_ = def.walk;
		runAnimSet_ = def.run;
		turnAnimSet_ = def.turn;
	}

	// 毎フレーム、PlayerStatusController::HandleMovementInputから呼ばれる。
	// inputDirectionは正規化されていなくてもよい(内部で正規化する)。
	void Tick(float deltaTime, MovementState state, const Math::Vector3& inputDirection, bool isLockedOn);

	// 戦闘行動からの復帰直後(StateNone::Enter)に、入力状態を反映しつつ
	// アニメーション・向きを同期し直す(PlayerStatusController::
	// RefreshMovementAnimationから呼ばれる)。
	void Refresh(MovementState state, const Math::Vector3& inputDirection, bool isLockedOn);

private:
	enum class Phase { Turning, Start, Loop, End };
	enum class LoopSource { WalkForward, WalkLocked, Run };

	void AdvancePhaseTiming(float deltaTime);
	void UpdateTurning(float deltaTime);

	void BeginTurnOrStart(MovementState state, const Math::Vector3& horizontalDir, bool allowTurn);
	void BeginStart(MovementState state);
	void SwitchLoopWalkForward();
	void SwitchLoopWalkLocked(Direction8 dir);
	void SwitchLoopRun();
	void BeginEnd();
	void CompleteStartToLoop();

	float CurrentStartDuration() const;
	float CurrentEndDuration() const;
	const MotionClipData& ResolveTurnData(TurnDirection turn) const;

	void FaceDirection(const Math::Vector3& horizontalDir);

	void Play(const std::string& name, bool loop, float targetDurationSeconds = -1.0f,
		bool useRootMotion = false, bool extractRotation = false);

	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;
	TransformComponent* transform_ = nullptr;

	MovementState lastState_ = MovementState::Stand;
	bool lastLockedOn_ = false;

	Phase phase_ = Phase::Loop;
	float elapsed_ = 0.0f;

	LoopSource loopSource_ = LoopSource::WalkForward;
	Direction8 lockedDirection_ = Direction8::Forward;

	Math::Quaternion turnStartRotation_;
	Math::Quaternion turnTargetRotation_;
	float turnDuration_ = 0.2f;
	MovementState pendingStateAfterTurn_ = MovementState::Walk;

	// SetMovementAnimations()で注入されるまでは空(アニメーション名が
	// 空文字列)のまま。PlayerCombatMovementComponent::walkSpeed_/runSpeed_
	// と同じ理由で、中途半端な既定値をここに直書きしない
	// (呼び忘れがあれば「アニメーションが再生されない」という分かりやすい
	//  形で不具合に気付ける)。以前ここにあった具体的な値
	// (APose_Strafe_Walk_*/APose_Strafe_Run_*/APose_TurnXxx)は
	// PlayerCombatDataTable::CreateDebugPlayerMovementAnimations()へ移設した。
	WalkAnimationSet walkAnimSet_;
	MovementPhaseClips runAnimSet_;
	TurnAnimationSet turnAnimSet_;

	static constexpr const char* kIdleAnimation = "APose_Idle";
	static constexpr float kBlendDuration = 0.15f;
	static constexpr const char* kRootMotionBoneName = "mixamorig_Hips";
};