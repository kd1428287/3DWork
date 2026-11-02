// PlayerInputComponent.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "Application/Definitions/Character/Player/PlayerInputTypes.h"
#include "Application/Core/EventBus/EventBus.h"
#include "Application/Core/EventBus/Events/InputLockEvents.h"
#include "../../../Tags/IMovementSource.h"

#include "../../Camera/CameraComponent.h"
#include "../../Camera/CameraOrbitComponent.h"

#include "Application/Entity/InputBuffer.h"
#include "PlayerLockOnComponent.h"
#include "PlayerCommandResolver.h"
#include "KdPlayerInputDevice.h" // KdInputManagerが見える位置でinclude

#include <memory>

// ロック中限定の変換式。CameraComponent側の変換(オフセット方向→スクリーン前方向、
// のため反転が必要)とは意味が異なり、こちらは自身のforwardをそのままスクリーン前方向
// として使いたいため反転しない。
inline float ComputeYawFromOwnForward(const Math::Vector3& horizontalForward)
{
	return std::atan2(horizontalForward.x, horizontalForward.z);
}

// 入力の窓口。デバイスの読み取り→基準変換(カメラ/ロック)→コマンド化までを担い、
// 実行の判断はPlayerActionDispatcherに任せる。
class PlayerInputComponent : public ComponentBase, public IMovementSource {
public:
	explicit PlayerInputComponent(GameObject* owner)
		: ComponentBase(owner), device_(std::make_unique<KdPlayerInputDevice>()) {}

	void Awake()
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		lockOnComponent_ = GetOwner()->GetComponent<PlayerLockOnComponent>();

		inputLockSub_ = GLOBALEVENT->SubscribeScoped<Events::InputLockChanged>(
			[this](const Events::InputLockChanged& e) { SetInputLocked(e.locked); });
	}

	// リプレイ・テスト・AI操作用に生入力の取得元を差し替える。
	void SetInputDevice(std::unique_ptr<IPlayerInputDevice> device) { device_ = std::move(device); }

	// ロック中は入力を読まず、保持状態と先行入力をすべて空にする
	void SetInputLocked(bool locked) {
		if (inputLocked_ == locked) return;
		inputLocked_ = locked;
		if (locked) ClearInput();
	}

	// activeCameraが使えない/yawを決められない場合のフォールバック先(別GameObject上のためHandle保持)。
	void SetCameraOrbitFallback(Handle<CameraOrbitComponent> orbit) { cameraOrbitFallback_ = orbit; }

	// 毎フレームPreUpdateで呼ばれる。バッファの期限減算もここで行うため、
	// 今フレームに積んだ入力は丸々1フレーム分の猶予を持つ。
	void PreUpdate(float deltaTime) override
	{
		if (inputLocked_ || device_ == nullptr) return;

		const PlayerRawInput raw = device_->Poll();
		const Math::Vector3 dir{ raw.moveAxis.x, 0.0f, raw.moveAxis.y };

		const bool isLockedOn = lockOnComponent_ != nullptr && lockOnComponent_->IsLockedOn();

		// 移動(Walk)用の基準: ロック中かつWalkの場合だけ自身の向きを使う。
		// ロック中でもRunなら向きのバインドを外し、カメラ基準にフォールスルーする。
		Math::Quaternion moveBasis = Math::Quaternion::Identity;
		bool hasMoveBasis = false;
		if (isLockedOn && !raw.dashHeld && transform_ != nullptr) {
			Math::Vector3 basisForward = transform_->GetForward();
			basisForward.y = 0.0f;
			if (basisForward.LengthSquared() > kMinForwardLengthSq) {
				basisForward.Normalize();
				moveBasis = Math::Quaternion::CreateFromAxisAngle(
					Math::Vector3::Up, ComputeYawFromOwnForward(basisForward));
				hasMoveBasis = true;
			}
		}

		Math::Quaternion cameraBasis = Math::Quaternion::Identity;
		const bool hasCameraBasis = ResolveCameraBasis(cameraBasis);

		Math::Vector3 moveDir = dir;
		if (dir != Math::Vector3::Zero) {
			if (hasMoveBasis)        moveDir = Math::Vector3::Transform(dir, moveBasis);
			else if (hasCameraBasis) moveDir = Math::Vector3::Transform(dir, cameraBasis);
		}
		moveDir.Normalize();

		// 回避方向はロック中でも常にカメラ基準(自身の向き基準にはしない)
		Math::Vector3 evadeDir = dir;
		if (dir != Math::Vector3::Zero && hasCameraBasis) {
			evadeDir = Math::Vector3::Transform(dir, cameraBasis);
			evadeDir.Normalize();
		}

		intent_.moveDirection = moveDir;
		intent_.dashHeld = raw.dashHeld;
		intent_.guardHeld = raw.guardHeld;
		intent_.attackHeld = raw.attackHeld;

		resolver_.Update(deltaTime, raw, moveDir, evadeDir, buffer_);
	}

	// --- ディスパッチャ向け ---
	const PlayerIntent& GetIntent() const { return intent_; }
	InputBuffer<PlayerInputCommand>& GetBuffer() { return buffer_; }

	// --- 参照用アクセサ ---
	bool IsInputLocked() const { return inputLocked_; }
	bool IsGuardHeld() const { return intent_.guardHeld; }
	bool IsAttackHeld() const { return intent_.attackHeld; }
	bool IsDashHeld() const { return intent_.dashHeld; }
	MovementState GetDesiredMovementState() const { return intent_.DesiredMovementState(); }
	Math::Vector3 GetMoveDirection() const { return intent_.moveDirection; }

	// IMovementSourceの実装
	Math::Vector3 GetDesiredVelocity() override { return intent_.moveDirection; }

private:
	void ClearInput() {
		intent_ = PlayerIntent{};
		buffer_.ClearAll(); // ロック前に積まれた先行入力が演出中に発動しないよう捨てる
		resolver_.Reset();
	}

	bool ResolveCameraBasis(Math::Quaternion& outBasis) const
	{
		if (const SceneContext* context = GetOwner()->GetContext()) {
			if (CameraComponent* camera = context->activeCamera) {
				outBasis = camera->GetMovementYawRotation();
				return true;
			}
		}
		if (CameraOrbitComponent* orbit = cameraOrbitFallback_.Resolve()) {
			outBasis = Math::Quaternion::CreateFromYawPitchRoll(orbit->GetYaw(), 0.0f, 0.0f);
			return true;
		}
		return false;
	}

	std::unique_ptr<IPlayerInputDevice> device_;
	PlayerCommandResolver resolver_;
	InputBuffer<PlayerInputCommand> buffer_;
	PlayerIntent intent_;

	TransformComponent* transform_ = nullptr;
	PlayerLockOnComponent* lockOnComponent_ = nullptr;

	bool inputLocked_ = false;
	ScopedSubscriber inputLockSub_;

	Handle<CameraOrbitComponent> cameraOrbitFallback_;

	// 自身の水平前方ベクトルがこれ以下(ほぼ真上/真下)ならyawを決めずカメラ基準へフォールバックする閾値。
	static constexpr float kMinForwardLengthSq = 1e-6f;
};
