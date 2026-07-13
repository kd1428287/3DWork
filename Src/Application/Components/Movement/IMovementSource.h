#pragma once
#include "../Transform/TransformComponent.h" 
// ============================================================
// 移動方向・速度の「決定方法」を抽象化するインターフェース。
// MovementComponentはこれを通じて「今どちらに動きたいか」を
// 問い合わせるだけで、決定ロジック(手動入力かAIか)には関知しない。
//
// 実装例:
//   - PlayerInputComponent : 外部の入力システムから渡された値を返す(手動)
//   - AIWanderComponent    : 内部ロジックで自律的に決める(自動)
// ============================================================
class IMovementSource {
public:
	virtual ~IMovementSource() = default;

	// 「このフレームでどちらに、どれくらいの強さで動きたいか」を返す。
	// 戻り値は通常 -1.0〜1.0 程度の方向ベクトル(正規化された入力)を想定し、
	// 実際の速度への変換(speedを掛ける等)はMovementComponent側で行う。
	virtual Math::Vector3 GetDesiredVelocity(float deltaTime) = 0;
};