#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"

class MovementComponent;
class TweenMoveComponent;

// 初期設定(Prefab/JSONから渡す値)。既定値は意図的に0(未設定なら動かないので不具合に気付ける)。
struct PlayerCombatMovementConfig
{
	float walkSpeed = 0.0f;
	float runSpeed = 0.0f;
};

// ============================================================
// PlayerCombatMovementComponent
//
// 実際にTransformComponentを動かす側を担当する、
// PlayerStatusControllerの兄弟コンポーネント。
//
// 通常移動(Walk/Run)の速度適用と、攻撃/回避中の決め打ち踏み込み移動
// (TweenMoveComponentによるステップ)の両方をここに集約する。
// PlayerMovementAnimationComponentが「見た目(アニメーション・向き)」を
// 担当するのに対し、こちらは「実際の位置」を担当する対になる存在。
// 旧PlayerStatusController::ApplyMovementState/UpdateMovementState/
// RequestStepMove/RequestStepMoveTowardsTarget/CancelStepMove/
// walkSpeed_・runSpeed_をここへ集約する。
//
// 【データ駆動について】walkSpeed_/runSpeed_はこのクラス自身の既定値を
// 持たない(= constexprでハードコードしない)。PlayerDefinition::
// walkSpeed/runSpeedを唯一のデータ源とし、PlayerFactory構築時に
// SetMovementSpeeds()で注入してもらう(SetWeapon()と同じ、Start()前に
// 呼ばれる想定のセッター)。
// ============================================================
class PlayerCombatMovementComponent : public ComponentBase
{
public:
	using Config = PlayerCombatMovementConfig;

	explicit PlayerCombatMovementComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetConfig(const Config& config) { SetMovementSpeeds(config.walkSpeed, config.runSpeed); }

	void Start() override;

	void SetMovementSpeeds(float walkSpeed, float runSpeed) {
		walkSpeed_ = walkSpeed;
		runSpeed_ = runSpeed;
	}

	// 通常移動(Walk/Run)の目標速度をMovementComponentへ適用する。
	void ApplyMovementState(MovementState state);

	// CombatState::Noneの間、毎フレーム呼ばれる(スタミナ消費等、将来の
	// 拡張用のフック。現状は空実装)。
	void UpdateMovementState(MovementState state, float deltaTime);

	// FSMがNone以外へ遷移した際、通常移動の物理駆動(MovementComponent)を
	// 止める/Noneに戻った際に再度有効化するためのスイッチ
	// (旧movementComponent_->SetEnabled(...)相当)。
	void SetMovementEnabled(bool enabled);

	// 決め打ちの軌道で移動する
	void RequestStepMove(const Math::Vector3& direction, float distance, float duration);

	// targetが非nullptrなら、そこまでengageDistanceを残して詰める踏み込み
	// 移動を行う。targetがnullptr、または各種Transformが取得できない場合は
	// fallbackDirectionを使った決め打ち移動(RequestStepMove相当)に
	// フォールバックする。
	void RequestStepMoveTowardsTarget(GameObject* target, const Math::Vector3& fallbackDirection,
		float stepDistance, float engageDistance, float duration);

	void CancelStepMove();

private:
	MovementComponent* movementComponent_ = nullptr;

	// TweenMoveComponentはStart()で一度だけアタッチし、以降は
	// enabled_フラグ(TweenMoveComponent::SetEnabled/Cancel)で
	// 動作可否を切り替える(アタッチ/デタッチの繰り返しはしない)。
	TweenMoveComponent* tweenMoveComponent_ = nullptr;

	// SetMovementSpeeds()で注入されるまでは0のまま
	// (=呼び忘れがあれば「動かない」という分かりやすい形で不具合に気付ける。
	//  中途半端な既定値をここに書いて黙って動いてしまう方が発見しづらい)。
	float walkSpeed_ = 0.0f;
	float runSpeed_ = 0.0f;

	// ゼロ除算・方向未定義を避けるための数値的な安全マージン。
	// デザイナーが調整する類の値ではないため、データ化はしない。
	static constexpr float kDirectionEpsilon = 1e-6f;
};