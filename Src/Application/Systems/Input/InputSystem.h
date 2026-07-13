#pragma once
#include "../../Components/Player/PlayerInputComponent.h"

// ============================================================
// 入力システム。
//
// KdInputManager(実際のハードウェア入力を、名前付きのボタン/軸として
// まとめてくれるクラス)を読めるのはこのクラスだけ。
// 「"Move"という軸が(0.3, -1.0)を指している」という生の情報を
// 「この方向に動きたい」というゲーム的な意味(Vector3)に変換し、
// PlayerInputComponentへ渡す。ボタン系も同様に、
// 継続入力(Dash/Guard)はSetXxxHeld()で都度上書き、
// 単発入力(Attack/Evade/Parry)はPushCommand()でバッファへ積む形に変換する。
//
// キー/パッドとゲーム内動作の対応付け自体はKdInputCollector側
// (AddButton/AddAxisで名前付き登録する部分)の責務であり、
// InputSystemはその名前("Move"等)を問い合わせるだけでよい。
//
// ※ GetButtonPress/GetButtonTrigger は「押されている間ずっとtrue」
//   「押された瞬間の1フレームだけtrue」を返す想定のプレースホルダー名。
//   実際のKdInputManager側のAPI名に合わせて調整すること。
//
// この変換をInputSystemに閉じ込めておくことで、後で
//   - 軸の割り当てを変更したい
//   - リプレイ入力やネットワーク入力に差し替えたい
// となっても、PlayerInputComponent/MovementComponent側は無変更で済む。
//
// プレイヤーは数が少なく、生成タイミングも決まっているため、
// イベントバスを介さず「生成時に直接登録」する方式にしている。
// 対象が動的に増減するもの(カードなど)はEffectResolverSystemの
// ようにイベントバス経由にする。
// ============================================================
class InputSystem {
public:
	void RegisterPlayer(PlayerInputComponent* input) { playerInput_ = input; }

	void Update(float /*deltaTime*/) {
		if (playerInput_ == nullptr) return;

		// --- 移動方向 -------------------------------------------------
		// "Move"という名前で登録された軸入力(KdInputAxisForWindows等)を取得。
		// x:左右, y:前後 として扱う想定(実際の軸定義に合わせて調整する)。
		const Math::Vector2 axis = KdInputManager::Instance().GetAxisState("Move");
		playerInput_->SetMoveDirection({ axis.x, 0.0f, axis.y });

		// --- 継続入力(押している間ずっと反映) -----------------------------
		playerInput_->SetDashHeld(KdInputManager::Instance().IsHold("Dash"));
		playerInput_->SetGuardHeld(KdInputManager::Instance().IsHold("Guard"));

		// --- 単発入力(押した瞬間だけバッファへ積む) -------------------------
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

private:
	PlayerInputComponent* playerInput_ = nullptr;
};