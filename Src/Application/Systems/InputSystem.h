#pragma once
#include "../../Components/Character/Player/PlayerInputComponent.h"
#include "../../Components/Camera/CameraOrbitComponent.h"

// ============================================================
// 入力システム。
//
// KdInputManager(実際のハードウェア入力を、名前付きのボタン/軸として
// まとめてくれるクラス)を読めるのはこのクラスだけ。
// 「"Move"という軸が(0.3, -1.0)を指している」という生の情報を
// 「この方向に動きたい」というゲーム的な意味(Vector3)に変換し、
// PlayerInputComponentへ渡す。ボタン系も同様に、
// 継続入力(Dash/Guard)はSetXxxHeld()で都度上書き、
// 単発入力(Attack/Evade)はPushCommand()でバッファへ積む形に変換する。
//
// マウスの"Look"軸(main.cppで KdInputAxisForWindowsMouse として登録済み)は
// 1フレームの移動量(ピクセル)をそのままCameraOrbitComponentへ注入する。
// 度数/ラジアンへの変換や感度調整はCameraOrbitComponent側の責務にしている。
//
// "Move"軸もCameraOrbitComponentが登録されていれば、そのyaw(水平方向)で
// 回転させてからPlayerInputComponentへ渡す。これにより「カメラの前方向が
// そのままキャラの前進方向になる」カメラ相対移動になる
// (CameraOrbitComponent未登録時はワールド軸基準のまま、従来通り)。
//
// キー/パッドとゲーム内動作の対応付け自体はKdInputCollector側
// (AddButton/AddAxisで名前付き登録する部分)の責務であり、
// InputSystemはその名前("Move"、"Look"等)を問い合わせるだけでよい。
//
// この変換をInputSystemに閉じ込めておくことで、後で
//   - 軸の割り当てを変更したい
//   - リプレイ入力やネットワーク入力に差し替えたい
// となっても、PlayerInputComponent/CameraOrbitComponent側は無変更で済む。
//
// プレイヤー・カメラともに数が少なく、生成タイミングも決まっているため、
// イベントバスを介さず「生成時に直接登録」する方式にしている。
// 対象が動的に増減するもの(カードなど)はEffectResolverSystemの
// ようにイベントバス経由にする。
// ============================================================
class InputSystem {
public:
	void RegisterPlayer(PlayerInputComponent* input) { playerInput_ = input; }
	void RegisterCameraOrbit(CameraOrbitComponent* orbit) { cameraOrbit_ = orbit; }

	void Update(float /*deltaTime*/) {
		if (playerInput_ != nullptr) {
			// --- 移動方向 ---------------------------------------------
			// "Move"という名前で登録された軸入力(KdInputAxisForWindows等)を取得。
			// x:左右, y:前後 のローカルな移動意思として扱う。
			const Math::Vector2 axis = KdInputManager::Instance().GetAxisState("Move");
			Math::Vector3 moveDir{ axis.x, 0.0f, axis.y };

			// カメラの水平方向(yaw)を移動方向の基準にする(カメラ相対移動)。
			// ピッチ(見上げ/見下ろし)は意図的に含めない。含めてしまうと、
			// 見上げている間に前進しようとした時にキャラが宙(または地面)へ
			// 向かおうとしてしまうため。
			//
			// CameraOrbitComponentが未登録(固定カメラ等)の場合は、従来通り
			// ワールド軸に対する入力としてそのまま扱う。
			if (cameraOrbit_ != nullptr) {
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
			// パリィは独立した入力を持たない。Guardボタン押下(SetGuardHeld)の
			// 開始直後の数フレームが、そのままパリィ判定窓になる
			// (PlayerStatusController::IsInParryWindow()参照)。
		}

		if (cameraOrbit_ != nullptr) {
			// --- マウス視点回転 -----------------------------------------
			// "Look"軸(KdInputAxisForWindowsMouse)は1フレームの移動量(ピクセル)を
			// そのまま返す。ラジアンへの変換・感度・ピッチのクランプは
			// CameraOrbitComponent側の責務。
			const Math::Vector2 look = KdInputManager::Instance().GetAxisState("Look");
			cameraOrbit_->SetLookDelta(look);
		}
	}

private:
	PlayerInputComponent* playerInput_ = nullptr;
	CameraOrbitComponent* cameraOrbit_ = nullptr;
};