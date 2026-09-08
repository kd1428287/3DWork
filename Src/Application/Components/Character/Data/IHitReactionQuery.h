#pragma once

class IHitReactionQuery
{
public:
	virtual ~IHitReactionQuery() = default;

	// 今パリィ猶予中か(Guard開始直後の弾き判定ウィンドウ)。
	virtual bool IsInParryWindow() const = 0;

	// 今ガード中か(パリィ猶予を過ぎた通常ブロック状態も含む)。
	virtual bool IsGuarding() const = 0;

	// 通常被弾によるスタン相当の反応に入ってほしい、という通知。
	virtual void EnterStagger(bool isLarge, float duration) = 0;

	// パリィが成立した、という通知(自分自身がパリィした側)。
	// 専用の成功モーションを再生してほしい合図として使う。
	virtual void NotifyParrySuccess() = 0;

	// 通常ブロックで被弾した、という通知(自分自身がガードした側)。
	// パリィ成功ほど特別な演出ではないが、被弾の都度ヒットリアクションの
	// アニメーションを再生してほしい合図として使う。
	virtual void NotifyGuardHit() = 0;
};