#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "../../../Tags/IMovementSource.h"

#include "../../Camera/CameraComponent.h"
#include "../../Camera/CameraOrbitComponent.h"

#include "../Common/CharacterInputBufferComponent.h"
#include "PlayerLockOnComponent.h" 

// ロック中限定の変換式。CameraComponent側の変換(オフセット方向→スクリーン前方向、
// のため反転が必要)とは意味が異なり、こちらは自身のforwardをそのままスクリーン前方向
// として使いたいため反転しない。
static float ComputeYawFromOwnForward(const Math::Vector3& horizontalForward)
{
	return std::atan2(horizontalForward.x, horizontalForward.z);
}

class PlayerInputComponent : public ComponentBase, public IMovementSource {
public:
	explicit PlayerInputComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake()
	{
		actionBuffer_ = GetOwner()->GetComponent<PlayerActionBufferComponent>();
		if (actionBuffer_ == nullptr) {
			actionBuffer_ = GetOwner()->RequestAddComponent<PlayerActionBufferComponent>();
		}
		transform_ = GetOwner()->GetComponent<TransformComponent>();       // 追加
		lockOnComponent_ = GetOwner()->GetComponent<PlayerLockOnComponent>(); // 追加
	}

	// カメラのactiveCamera(SceneContext経由)が使えない/yawを決められない場合に
	// フォールバック先として参照するCameraOrbitComponent。
	// 別GameObject(カメラリグ)上のコンポーネントのためHandle<T>で保持する。
	void SetCameraOrbitFallback(Handle<CameraOrbitComponent> orbit) { cameraOrbitFallback_ = orbit; }

	// GameObjectのUpdate、またはPreUpdateで毎フレーム呼び出す。
	// KdInputManagerから今フレームの生入力を読み取ってmoveDirection_/
	// dashHeld_等へ反映し、タップ入力はactionBuffer_へ積む。先行入力バッファ
	// 自体の寿命管理はCharacterInputBufferComponent::PostUpdate()側が行う
	// (このコンポーネントのPreUpdateより後に呼ばれるため、今フレーム
	//  積んだ分がその場で減算されてしまうことはない)。
	void PreUpdate(float deltaTime) override
	{
		const Math::Vector2 axis = KdInputManager::Instance().GetAxisState("Move");
		Math::Vector3 dir{ axis.x, 0.0f, axis.y };

		const bool isLockedOn = lockOnComponent_ != nullptr && lockOnComponent_->IsLockedOn();
		const bool wantsRun = KdInputManager::Instance().IsHold("Dash"); // dashHeld_確定より前に判定用として先読み

		// 移動(Walk)用の基準: ロック中かつWalkの場合だけ自身の向きを使う。
		// ロック中でもRunなら向きのバインドを外し、下のカメラ基準にフォールスルーする。
		Math::Quaternion moveBasis = Math::Quaternion::Identity;
		bool hasMoveBasis = false;
		if (isLockedOn && !wantsRun && transform_ != nullptr) {
			Math::Vector3 basisForward = transform_->GetForward();
			basisForward.y = 0.0f;
			if (basisForward.LengthSquared() > kMinForwardLengthSq) {
				basisForward.Normalize();
				const float yaw = ComputeYawFromOwnForward(basisForward);
				moveBasis = Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw);
				hasMoveBasis = true;
			}
		}

		// カメラ基準: Run時の移動、および回避方向(ロック中でも常に)から共通で参照する。
		Math::Quaternion cameraBasis = Math::Quaternion::Identity;
		bool hasCameraBasis = false;
		if (const SceneContext* context = GetOwner()->GetContext()) {
			if (CameraComponent* camera = context->activeCamera) {
				cameraBasis = camera->GetMovementYawRotation();
				hasCameraBasis = true;
			}
		}
		if (!hasCameraBasis) {
			if (CameraOrbitComponent* orbit = cameraOrbitFallback_.Resolve()) {
				cameraBasis = Math::Quaternion::CreateFromYawPitchRoll(orbit->GetYaw(), 0.0f, 0.0f);
				hasCameraBasis = true;
			}
		}

		Math::Vector3 moveDir = dir;
		if (dir != Math::Vector3::Zero) {
			if (hasMoveBasis)        moveDir = Math::Vector3::Transform(dir, moveBasis);
			else if (hasCameraBasis) moveDir = Math::Vector3::Transform(dir, cameraBasis);
		}
		SetMoveDirection(moveDir);

		// 回避方向はロック中でも常にカメラ基準(自身の向き基準にはしない)
		Math::Vector3 evadeDir = dir;
		if (dir != Math::Vector3::Zero && hasCameraBasis) {
			evadeDir = Math::Vector3::Transform(dir, cameraBasis);
			evadeDir.Normalize();
		}

		dashHeld_ = wantsRun;
		guardHeld_ = KdInputManager::Instance().IsHold("Guard");
		attackHeld_ = KdInputManager::Instance().IsHold("Attack");

		if (actionBuffer_ != nullptr) {
			if (KdInputManager::Instance().IsPress("Attack")) {
				actionBuffer_->PushCommand(ActionCommand::Attack, moveDirection_);
			}
			if (KdInputManager::Instance().IsPress("Evade")) {
				actionBuffer_->PushCommand(ActionCommand::Evade, evadeDir); // moveDirection_ではなくevadeDirを積む
			}
			if (KdInputManager::Instance().IsPress("Guard")) {
				actionBuffer_->PushCommand(ActionCommand::Guard, Math::Vector3::Zero);
			}
		}
		if (KdInputManager::Instance().IsPress("Lock")) {
			lockPressed_ = true;
		}
	}

	// --- 外部（PlayerStatusControllerなど）が先行入力を確認/消費する関数 ---
	// 実体はactionBuffer_(CharacterInputBufferComponent)への薄い委譲。

	bool HasCommand(ActionCommand command) const {
		return actionBuffer_ != nullptr && actionBuffer_->HasCommand(command);
	}

	bool ConsumeCommand(ActionCommand command) {
		return actionBuffer_ != nullptr && actionBuffer_->ConsumeCommand(command);
	}

	// 方向スナップショットも合わせて取り出したい場合(Evade等)はこちら。
	bool ConsumeCommand(ActionCommand command, Math::Vector3& outDirection) {
		return actionBuffer_ != nullptr && actionBuffer_->ConsumeCommand(command, outDirection);
	}

	bool IsGuardHeld() const { return guardHeld_; }
	bool IsAttackHeld() const { return attackHeld_; }
	bool IsDashHeld() const { return dashHeld_; }

	// このフレームLockが押されていたかを取り出し、同時にフラグを消費(false)する。
	// HasCommand()に相当する「覗き見だけ」の版は用意していない。Lockは
	// CanStart*系のような実行可否判定を挟まず常に処理してよい入力のため、
	// ConsumeCommand()相当の一発だけで足りる。
	bool ConsumeLockPressed() {
		bool pressed = lockPressed_;
		lockPressed_ = false;
		return pressed;
	}

	// 移動方向とDashキーの状態から、今フレームの移動意思をMovementStateとして
	// 解決する。キーボード操作前提のため、アナログの傾き量ではなく
	// Dashキーが押されているか否かでWalk/Runを切り替える。
	MovementState GetDesiredMovementState() const {
		if (moveDirection_.LengthSquared() <= 0.0f) return MovementState::Stand;
		return dashHeld_ ? MovementState::Run : MovementState::Walk;
	}

	// IMovementSourceの実装
	Math::Vector3 GetDesiredVelocity() override {
		return moveDirection_;
	}

	// moveDirection_の読み取り専用アクセサ。GetDesiredVelocity()は
	// IMovementSource側の都合でconstにできないため、const参照からも
	// 呼べる版を別途用意する(PlayerStatusController::HandleMovementInput
	// がWalk中の前後左右判定に使う。ClassifyEvadeDirection参照)。
	Math::Vector3 GetMoveDirection() const { return moveDirection_; }

private:
	void SetMoveDirection(Math::Vector3 direction) {
		direction.Normalize();
		moveDirection_ = direction;
	}

	PlayerActionBufferComponent* actionBuffer_ = nullptr;
	TransformComponent* transform_ = nullptr;           // 追加
	PlayerLockOnComponent* lockOnComponent_ = nullptr;   // 追加

	Math::Vector3 moveDirection_;
	bool dashHeld_ = false;
	bool guardHeld_ = false;
	bool attackHeld_ = false;
	bool lockPressed_ = false; // このフレームLockが押されたか(消費されるまで保持)

	Handle<CameraOrbitComponent> cameraOrbitFallback_; // activeCamera不使用時のyawフォールバック先

	// 自身/カメラの水平前方ベクトルがこれ以下(ほぼ真上/真下を向いている)
	// の場合は、そこからyawを決めずCameraOrbitComponent側にフォールバックする、
	// という閾値。
	static constexpr float kMinForwardLengthSq = 1e-6f;
};