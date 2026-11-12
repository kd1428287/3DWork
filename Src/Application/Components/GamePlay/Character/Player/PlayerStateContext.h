// PlayerStateContext.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"

class PlayerInputComponent;
class PlayerLockOnComponent;
class PlayerMovementAnimationComponent;
class ModelAnimatorComponent;
class WeaponSetComponent;
class PlayerAttackSelector;
class PlayerFacingComponent;
class PlayerCombatMovementComponent;
class PlayerInkGaugeComponent;

// 攻撃idを直接指定して攻撃へ遷移する要求(チャージ攻撃など)。
struct PlayerAttackRequest
{
	std::string attackId;
	float damageScale = 1.0f;
	float chargeRatio = 0.0f; // チャージ攻撃の溜め率(0〜1)。通常は0
};

// Stateからの状態遷移要求の受け口(PlayerStatusControllerが実装する)。
class IPlayerStateRequester
{
public:
	virtual ~IPlayerStateRequester() = default;
	virtual void ChangeStateToNone() = 0;

	// 開始できなければ遷移せずfalseを返す(呼び出し側が代替行動を決める)。
	virtual bool RequestAttack(const PlayerAttackRequest& request) = 0;
};

// Stateが依存する協調コンポーネントと共有状態をまとめた束。
// ポインタはPlayerStatusController::Awakeで設定され、Stateはこれだけを知ればよい。
struct PlayerStateContext
{
	GameObject* owner = nullptr;
	PlayerInputComponent* input = nullptr;
	PlayerLockOnComponent* lockOn = nullptr;
	PlayerMovementAnimationComponent* movementAnimation = nullptr;
	ModelAnimatorComponent* animator = nullptr;
	WeaponSetComponent* weaponSet = nullptr;
	PlayerAttackSelector* attackSelector = nullptr;
	PlayerFacingComponent* facing = nullptr;
	PlayerCombatMovementComponent* combatMovement = nullptr;
	IPlayerStateRequester* requester = nullptr;
	PlayerInkGaugeComponent* inkGauge = nullptr;

	MovementState movementState = MovementState::Stand;

	static constexpr float kDefaultAnimationBlendDuration = 0.35f;

	bool IsLockedOn() const;

	// --- 複数コンポーネントにまたがる/処理を含むものだけここに置く ---
	void PlayAnimation(const std::string& name, bool loop, float targetDurationSeconds,
		float startTime, float endTime,
		bool useRootMotion, float blendDurationSeconds);
	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f,
		bool useRootMotion = false, float blendDurationSeconds = kDefaultAnimationBlendDuration);
	void PlayAnimation(const MotionClipData& clip);

	void PlaySE(const std::string& id) const;

	// 現在の攻撃対象(ロック/正対中)へ向けた踏み込みを依頼する。
	void RequestStepMoveTowardsTarget(const Math::Vector3& fallbackDirection, float stepDistance,
		float engageDistance, float duration);
	void RequestRootMotionWarpTowardsTarget(float expectedDistance, float engageDistance,
		float minScale, float maxScale);
};
