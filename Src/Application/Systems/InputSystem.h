//#pragma once
//#include <cmath>
//#include "../../Components/Character/Player/PlayerInputComponent.h"
//#include "../../Components/Camera/CameraOrbitComponent.h"
//#include "../../Components/Camera/CameraComponent.h"
//#include "../../Core/SceneContext.h"
//#include "../Editor/EditorViewport.h"
//
//// KdInputManagerから得た実入力情報をゲーム用に変換する入力システム
//class InputSystem {
//public:
//	void RegisterPlayer(PlayerInputComponent* input) { playerInput_ = input; }
//	void RegisterCameraOrbit(CameraOrbitComponent* orbit) { cameraOrbit_ = orbit; }
//	void RegisterObjectManager(ObjectManager* obj) { objManager_ = obj; }
//
//	void Update(float /*deltaTime*/) {
//		
//
//		if (cameraOrbit_ != nullptr) {
//			// --- マウス視点回転 -----------------------------------------
//			const Math::Vector2 look = KdInputManager::Instance().GetAxisState("Look");
//			cameraOrbit_->SetLookDelta(look);
//		}
//
//		if (KdInputManager::Instance().IsPress("Pause")) {
//			static bool flg = true;
//			flg = !flg;
//			KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", flg);
//
//			flg ?
//				objManager_->AddMask(ObjectFlags::Gameplay) :
//				objManager_->RemoveMask(ObjectFlags::Gameplay);
//		}
//
//		if (KdInputManager::Instance().IsPress("Editor")) {
//			// エディタ描画のON/OFFを切り替え
//			EditorViewport::Instance().ToggleEnabled();
//
//			bool flg = EditorViewport::Instance().IsEnabled();
//			KdInputManager::Instance().SetAxisConfineToWindowCenter("Look", !flg);
//
//			// エディタOFF中(プレイ中)はカーソルを隠し、ON中(編集中)は表示する
//			ShowCursor(flg);
//
//			!flg ?
//				objManager_->AddMask(ObjectFlags::Gameplay) :
//				objManager_->RemoveMask(ObjectFlags::Gameplay); 
//		}
//	}
//
//private:
//	PlayerInputComponent* playerInput_ = nullptr;
//	CameraOrbitComponent* cameraOrbit_ = nullptr;
//	ObjectManager* objManager_ = nullptr;
//
//	// activeCameraの水平前方ベクトルがこれ以下(ほぼ真上/真下を向いている)
//	// の場合は、そこからyawを決めずCameraOrbitComponent側にフォールバックする、
//	// という閾値。
//	static constexpr float kMinCameraForwardLengthSq = 1e-6f;
//};