#pragma once
#include "IMovementSource.h"

// ============================================================
// 手動操作用コンポーネント。
//
// このクラス自体はキーボードやゲームパッドを直接読みには行かない。
// 「入力デバイスを読む処理」と「読んだ結果を保持して返す処理」を分けることで、
// キーボード・ゲームパッド・タッチ・リプレイ再生など、
// 入力ソースが変わってもMovementComponent側は一切変更不要になる。
//
// 想定される使い方:
//   auto* input = player->AddComponent<PlayerInputComponent>();
//   auto* move  = player->AddComponent<MovementComponent>(3.0f);
//   move->SetMovementSource(input);
//
//   // 毎フレーム、実際の入力ポーリング処理からここに書き込む
//   input->SetMoveDirection({ ReadHorizontalAxis(), 0.0f, ReadVerticalAxis() });
// ============================================================
class PlayerInputComponent : public ComponentBase, public IMovementSource {
public:
	explicit PlayerInputComponent(GameObject* owner) : ComponentBase(owner) {}

	// 入力ポーリング処理(キー入力・パッド入力など)から毎フレーム呼ばれる想定。
	// 通常は -1.0〜1.0 に正規化した値を渡す。
	void SetMoveDirection(Math::Vector3 direction) { moveDirection_ = direction; }

	Math::Vector3 GetDesiredVelocity(float /*deltaTime*/) override {
		return moveDirection_;
	}

private:
	Math::Vector3 moveDirection_;
};