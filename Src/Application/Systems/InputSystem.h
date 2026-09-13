#pragma once
#include <cmath>
#include "../../Components/Character/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraOrbitComponent.h"
#include "../../Components/Camera/CameraComponent.h"
#include "../../Core/SceneContext.h"
#include "../Editor/EditorViewport.h"

// KdInputManagerから得た実入力情報をゲーム用に変換する入力システム
class InputSystem {
public:
	void RegisterPlayer(PlayerInputComponent* input) { playerInput_ = input; }
	void RegisterCameraOrbit(CameraOrbitComponent* orbit) { cameraOrbit_ = orbit; }
	void RegisterObjectManager(ObjectManager* obj) { objManager_ = obj; }

	void Update(float /*deltaTime*/) {
		if (playerInput_ != nullptr) {
			// --- 移動方向 ---------------------------------------------
			const Math::Vector2 axis = KdInputManager::Instance().GetAxisState("Move");
			Math::Vector3 moveDir{ axis.x, 0.0f, axis.y };

			// カメラの水平方向(yaw)を移動方向の基準にする
			bool usedActualCameraForward = false;
			if (SceneContext* context = playerInput_->GetOwner()->GetContext()) {
				if (CameraComponent* camera = context->activeCamera) {
					Math::Vector3 camForward = camera->GetForward();
					camForward.y = 0.0f;
					if (camForward.LengthSquared() > kMinCameraForwardLengthSq) {
						camForward.Normalize();
						const float yaw = std::atan2(-camForward.x, -camForward.z);
						const Math::Quaternion yawOnly =
							Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw);
						moveDir = Math::Vector3::Transform(moveDir, yawOnly);
						usedActualCameraForward = true;
					}
				}
			}

			// activeCameraが無い、あるいはカメラがほぼ真上/真下を向いていて
			// yawを決められない場合は、従来通りCameraOrbitComponentの
			// 軌道角度にフォールバックする(CameraOrbitComponent未登録
			// (固定カメラ等)の場合は、ワールド軸に対する入力としてそのまま扱う)。
			if (!usedActualCameraForward && cameraOrbit_ != nullptr) {
				const Math::Quaternion yawOnly =
					Math::Quaternion::CreateFromYawPitchRoll(cameraOrbit_->GetYaw(), 0.0f, 0.0f);
				moveDir = Math::Vector3::Transform(moveDir, yawOnly);
			}

			playerInput_->SetMoveDirection(moveDir);

			// --- 継続入力(押している間ずっと反映) -------------------------
			playerInput_->SetDashHeld(KdInputManager::Instance().IsHold("Dash"));
			playerInput_->SetGuardHeld(KdInputManager::Instance().IsHold("Guard"));

			// --- 単発入力(押した瞬間だけバッファへ積む) ---------------------
			if (KdInputManager::Instance().IsPress("Attack")) {
				playerInput_->PushCommand(ActionCommand::Attack);
			}
			if (KdInputManager::Instance().IsPress("Evade")) {
				playerInput_->PushCommand(ActionCommand::Evade);
			}
			if (KdInputManager::Instance().IsPress("Lock")) {
				playerInput_->SetLockPressed();
			}
		}

		if (cameraOrbit_ != nullptr) {
			// --- マウス視点回転 -----------------------------------------
			const Math::Vector2 look = KdInputManager::Instance().GetAxisState("Look");
			cameraOrbit_->SetLookDelta(look);
		}

		if (KdInputManager::Instance().IsPress("Pause")) {
			static bool flg = true;
			flg = !flg;
			KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", flg);

			flg ?
				objManager_->AddMask(ObjectFlags::Gameplay) :
				objManager_->RemoveMask(ObjectFlags::Gameplay);
		}

		if (KdInputManager::Instance().IsPress("Editor")) {
			// エディタ描画のON/OFFを切り替え
			EditorViewport::Instance().ToggleEnabled();

			bool flg = EditorViewport::Instance().IsEnabled();
			KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", !flg);

			// エディタOFF中(プレイ中)はカーソルを隠し、ON中(編集中)は表示する
			ShowCursor(flg);

			!flg ?
				objManager_->AddMask(ObjectFlags::Gameplay) :
				objManager_->RemoveMask(ObjectFlags::Gameplay); 
		}
	}

private:
	PlayerInputComponent* playerInput_ = nullptr;
	CameraOrbitComponent* cameraOrbit_ = nullptr;
	ObjectManager* objManager_ = nullptr;

	// activeCameraの水平前方ベクトルがこれ以下(ほぼ真上/真下を向いている)
	// の場合は、そこからyawを決めずCameraOrbitComponent側にフォールバックする、
	// という閾値。
	static constexpr float kMinCameraForwardLengthSq = 1e-6f;
};