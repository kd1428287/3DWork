#pragma once

#include "../../Tags/IMovementSource.h"

// ============================================================
// MovementComponent(内力)
//
// 「キャラ自身の意思による移動」の速度を保持するだけのコンポーネント。
// IMovementSource(入力 / AI)から正規化された移動方向を受け取り、
// speed_を掛けた速度として公開する。
//
// 自分ではTransformを一切書き換えない。実際の位置の確定は
// MotionComposerComponentが、外力(VelocityComponent)・Tweenと
// 合成した上で1箇所で行う。
//
// speed_が「今いくつであるべきか」(Walk/Run、ガード中の減速、
// 状態異常による鈍化など)は、このコンポーネントの関知するところ
// ではない。PlayerStatusController等のState側が毎フレーム裁定して
// SetSpeed()で渡す。そのためMovementComponentはWalk/Runという
// 概念自体を知らず、Player/Enemyのどちらにもそのまま使える。
// ============================================================
class MovementComponent : public ComponentBase {
public:
	explicit MovementComponent(GameObject* owner, float speed = 1.0f)
		: ComponentBase(owner), speed_(speed) {
	}

	void Awake() override
	{
		// 自オブジェクトに入力ソースがあるならそちらを優先
		auto sources = GetOwner()->GetTagged<IMovementSource>();
		assert(sources.size() <= 1 && "MovementComponent: 複数のIMovementSourceが見つかりました");
		if (!sources.empty())
		{
			source_ = sources.front();
		}
	}

	// 入力ソースから今フレームの移動方向を取り込む。
	void FetchDesiredVelocity()
	{
		// source_が無いか非アクティブのときゼロで上書きする
		if (!source_ || !IsEnabled())
		{
			desiredVelocity_ = Math::Vector3::Zero;
			return;
		}

		desiredVelocity_ = source_->GetDesiredVelocity();
	}

	// 動きの決定方法(手動入力 / AI など)を差し替える。
	void SetMovementSource(IMovementSource* source) { source_ = source; }

	void SetSpeed(float speed) { speed_ = speed; }
	float GetSpeed() const { return speed_; }

	// 方向(正規化済み)そのもの。FacingDirectionComponentが
	// 「どちらを向くべきか」の根拠として参照する
	// (位置差分からの逆算ではなく、入力の意図を直接読むため)。
	const Math::Vector3& GetDesiredDirection() const { return desiredVelocity_; }

	Math::Vector3 GetVelocity() const { return desiredVelocity_ * speed_; }

private:
	float speed_;
	Math::Vector3 desiredVelocity_ = Math::Vector3::Zero;
	IMovementSource* source_ = nullptr;
};