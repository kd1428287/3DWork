#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"

class PlayerLockOnComponent;
class FacingDirectionComponent;

// ============================================================
// PlayerFacingComponent
//
// 攻撃対象・ロック対象へ「向く」ことだけを担当する、
// PlayerStatusControllerの兄弟コンポーネント。
//
// PlayerLockOnComponentが「誰をロックするか」の選定と保持
// (SceneContext::lockedTarget)を担当するのに対し、こちらは「その対象へ
// 実際にTransformを向ける」計算だけを持つ。旧PlayerStatusController::
// FaceAttackTarget/FaceTowards/UpdateLockOnFacing/currentAttackTarget_/
// ClassifyEvadeDirectionをここへ集約する。
//
// 【現状】骨格(公開インターフェース)のみ。Transformの回転計算・
// FacingDirectionComponentとの連携は次のステップで実装する。
// ============================================================
class PlayerFacingComponent : public ComponentBase
{
public:
	explicit PlayerFacingComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override;

	// ロック中ならロック対象へ、未ロックなら画面中心に最も近い敵へ正対する
	// (PlayerStatusController::FaceAttackTarget、StateAttack::Enter経由で
	// 呼ばれる)。結果はGetCurrentAttackTarget()から取得できる
	// (RequestStepMoveTowardsTargetが目標地点の計算に使う)。
	void FaceAttackTarget();

	// PlayerStatusController::Update()から、CombatState::Noneの間だけ
	// 毎フレーム呼ばれる。ロック中は入力方向に関わらずロック対象へ
	// 向きを固定する(isRunning=trueの間、すなわち走行中は方向ロックを
	// 解除する)。
	void UpdateLockOnFacing(bool isRunning);

	// 現在の前方と入力方向を比較し、前後左右のどれに該当するかを判定する
	// (StateEvade::Enter()がアニメーション選択に使う)。
	EvadeDirection ClassifyEvadeDirection(const Math::Vector3& inputDirection) const;

	// FaceAttackTarget()で正対した対象(未設定ならnullptr)。
	GameObject* GetCurrentAttackTarget() const { return currentAttackTarget_.Resolve(); }

	// PlayerStatusController::HandleMovementInput/OnStateChangedから、
	// 向きの自動追従(FacingDirectionComponent)を今フレーム有効にして
	// よいかを伝える。
	void SetFacingEnabled(bool enabled);

	void ClearAttackFacingOverride();

private:
	void FaceTowards(GameObject* target);

	TransformComponent* transform_ = nullptr;
	FacingDirectionComponent* facingDirectionComponent_ = nullptr;
	PlayerLockOnComponent* lockOnComponent_ = nullptr;

	Handle<GameObject> currentAttackTarget_;

	// ゼロ除算・方向未定義を避けるための数値的な安全マージン。
	// デザイナーが調整する類の値ではないため、データ化はしない
	// (PlayerCombatMovementComponent::kDirectionEpsilonと同じ位置づけ)。
	static constexpr float kDirectionEpsilon = 1e-6f;
};