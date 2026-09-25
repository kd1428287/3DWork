#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "../../../Tags/IMovementSource.h"

#include "../../Camera/CameraComponent.h"
#include "../../Camera/CameraOrbitComponent.h"

#include "../Common/CharacterInputBufferComponent.h"
#include "PlayerLockOnComponent.h" 

static float ComputeMovementBasisYaw(const Math::Vector3& horizontalForward)
{
	return std::atan2(-horizontalForward.x, -horizontalForward.z);
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

		if (dir != Math::Vector3::Zero)
		{
			Math::Vector3 basisForward;
			bool hasBasis = false;

			if (lockOnComponent_ != nullptr && lockOnComponent_->IsLockedOn() && transform_ != nullptr) {
				// ロック中: ClassifyDirection8/ClassifyEvadeDirectionが
				// transform_->GetForward()を基準に判定するのに合わせる
				basisForward = transform_->GetForward();
				basisForward.y = 0.0f;
				hasBasis = basisForward.LengthSquared() > kMinCameraForwardLengthSq;
			}
			else if (const SceneContext* context = GetOwner()->GetContext()) {
				if (CameraComponent* camera = context->activeCamera) {
					basisForward = camera->GetForward();
					basisForward.y = 0.0f;
					hasBasis = basisForward.LengthSquared() > kMinCameraForwardLengthSq;
				}
			}

			if (hasBasis) {
				basisForward.Normalize();
				const float yaw = ComputeMovementBasisYaw(basisForward); // ← 両方これ1本
				const Math::Quaternion yawOnly =
					Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw);
				dir = Math::Vector3::Transform(dir, yawOnly);
			}
			else if (CameraOrbitComponent* orbit = cameraOrbitFallback_.Resolve()) {
				const Math::Quaternion yawOnly =
					Math::Quaternion::CreateFromYawPitchRoll(orbit->GetYaw(), 0.0f, 0.0f);
				dir = Math::Vector3::Transform(dir, yawOnly);
			}
		}

		SetMoveDirection(dir);

		// --- 継続入力(押している間ずっと反映) -----------------------
		dashHeld_ = KdInputManager::Instance().IsHold("Dash");
		guardHeld_ = KdInputManager::Instance().IsHold("Guard");
		attackHeld_ = KdInputManager::Instance().IsHold("Attack");

		// --- 単発入力(押した瞬間だけバッファへ積む/フラグを立てる) -----
		if (actionBuffer_ != nullptr) {
			if (KdInputManager::Instance().IsPress("Attack")) {
				actionBuffer_->PushCommand(ActionCommand::Attack, moveDirection_);
			}
			if (KdInputManager::Instance().IsPress("Evade")) {
				actionBuffer_->PushCommand(ActionCommand::Evade, moveDirection_);
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

	// activeCameraの水平前方ベクトルがこれ以下(ほぼ真上/真下を向いている)
	// の場合は、そこからyawを決めずCameraOrbitComponent側にフォールバックする、
	// という閾値。
	static constexpr float kMinCameraForwardLengthSq = 1e-6f;
};