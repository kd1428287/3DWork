// PlayerCommandResolver.h
#pragma once
#include "Application/Definitions/Character/Player/PlayerInputTypes.h"
#include "Application/Entity/InputBuffer.h"

// 生入力の押下イベントをバッファ用コマンドへ変換する(エンジン非依存の純粋ロジック)。
// 同時押しの合成(Attack+Guard→Charge)と、保持が外れた入力の破棄もここで行う。
class PlayerCommandResolver
{
public:
	using Buffer = InputBuffer<PlayerInputCommand>;

	// フレーム頭に1回呼ぶ。moveDir/evadeDirは基準変換済みの正規化ベクトル。
	void Update(float deltaTime, const PlayerRawInput& raw,
		const Math::Vector3& moveDir, const Math::Vector3& evadeDir, Buffer& buffer);

	void Reset() { attackPressAge_ = kNever; }

private:
	static constexpr float kNever = 1000.0f;
	float attackPressAge_ = kNever;
};
