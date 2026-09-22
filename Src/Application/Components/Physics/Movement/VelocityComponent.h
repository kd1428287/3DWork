#pragma once

// 初期設定(Prefab/JSONから渡す値)。dampingPerSecondの意味はコンストラクタ参照。
struct VelocityConfig
{
	float dampingPerSecond = 0.05f;
};

// ============================================================
// VelocityComponent(外力)
//
// キャラ自身の意思とは無関係に働く力による「速度」を保持する。
//   impulseVelocity_   : ノックバック等の瞬間的な力。摩擦で減衰する。
//   continuousVelocity_: 重力等の継続的な力。自然減衰はしない。
//
// 自分ではTransformを一切書き換えない。実際の位置の確定は
// MotionComposerComponentが内力(MovementComponent)・Tweenと
// 合成した上で1箇所で行う。
// ============================================================
class VelocityComponent : public ComponentBase {
public:
	using Config = VelocityConfig;

	// dampingPerSecond: impulseVelocity_が1秒間でこの割合まで落ちる減衰係数。
	explicit VelocityComponent(GameObject* owner, float dampingPerSecond = VelocityConfig{}.dampingPerSecond)
		: ComponentBase(owner), dampingPerSecond_(dampingPerSecond) {
	}

	void SetConfig(const Config& config) { dampingPerSecond_ = config.dampingPerSecond; }

	// 摩擦減衰と、continuousVelocity_の上限クランプを行う。
	// MotionComposerComponentが、合成結果をTransformへ適用した「後」に呼ぶ
	// (適用前に呼ぶと、力が加わった最初の1フレームから既に減衰した値が
	//  使われてしまうため)。
	void DampingUpdate(float deltaTime)
	{
		// 摩擦減衰
		const float decay = std::pow(dampingPerSecond_, deltaTime);
		impulseVelocity_ *= decay;

		if (impulseVelocity_.LengthSquared() < kStopThresholdSq) {
			impulseVelocity_ = Math::Vector3::Zero;
		}

		// --- continuousVelocity_の上限クランプ -------------------------
		// MotionComposer側のクランプは「そのフレームに適用する移動量」に
		// 対して掛かるだけで、メンバの値自体は抑えられない。
		// 重力は接地するまで毎フレーム加算され続けるため、ここで
		// 値そのものを抑えておかないと、GroundSensorのレイ(checkDistance)を
		// 1フレームで飛び越えるほどの速度まで際限なく育ってしまう。
		if (continuousVelocity_.y < -kTerminalFallSpeed) {
			continuousVelocity_.y = -kTerminalFallSpeed;
		}

		// 水平成分(重力以外の継続的な外力。風・コンベア等を想定)も
		// 同様に上限を設ける。
		Math::Vector3 horizontal{ continuousVelocity_.x, 0.0f, continuousVelocity_.z };
		if (horizontal.LengthSquared() > kMaxContinuousHorizontalSpeed * kMaxContinuousHorizontalSpeed) {
			horizontal.Normalize();
			horizontal *= kMaxContinuousHorizontalSpeed;
			continuousVelocity_.x = horizontal.x;
			continuousVelocity_.z = horizontal.z;
		}
	}

	void AddImpulse(const Math::Vector3& impulse) { impulseVelocity_ += impulse; }
	void AddContinuousVelocity(const Math::Vector3& delta) { continuousVelocity_ += delta; }
	void SetContinuousVelocity(const Math::Vector3& velocity) { continuousVelocity_ = velocity; }
	const Math::Vector3& GetContinuousVelocity() const { return continuousVelocity_; }

	void ClearContinuousVelocity() { continuousVelocity_ = Math::Vector3::Zero; }
	void SetImpulseVelocity(const Math::Vector3& velocity) { impulseVelocity_ = velocity; }
	const Math::Vector3& GetImpulseVelocity() const { return impulseVelocity_; }

	Math::Vector3 GetVelocity() const { return impulseVelocity_ + continuousVelocity_; }

	bool IsMoving() const {
		return (impulseVelocity_ + continuousVelocity_).LengthSquared() > kStopThresholdSq;
	}

	bool IsImpulseActive() const {
		return impulseVelocity_.LengthSquared() > kStopThresholdSq;
	}

	void SetDampingPerSecond(float damping) { dampingPerSecond_ = damping; }
	float GetDampingPerSecond() const { return dampingPerSecond_; }

private:
	static constexpr float kStopThresholdSq = 0.0001f;

	// 落下の終端速度。GroundSensorComponent::checkDistance(既定0.15)を
	// 1フレームで飛び越えて接地を取りこぼさないよう、
	// 「terminalFallSpeed × 想定フレーム時間 < checkDistance」を
	// 目安に決めること(例: 20.0 × 1/30秒 ≒ 0.67 のため、
	// 高速落下を許すならcheckDistance側も合わせて広げる必要がある)。
	static constexpr float kTerminalFallSpeed = 20.0f;

	// 重力以外の継続的な外力(風・コンベア等)の水平成分の上限。
	static constexpr float kMaxContinuousHorizontalSpeed = 25.0f;

	Math::Vector3 impulseVelocity_ = Math::Vector3::Zero;
	Math::Vector3 continuousVelocity_ = Math::Vector3::Zero;
	float dampingPerSecond_;
};