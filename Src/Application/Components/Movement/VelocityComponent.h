#pragma once
#include "../Transform/TransformComponent.h"

// ============================================================
// 瞬間的な外力(ノックバック、パリィ反動など)を受けて、
// 摩擦で減衰しながら流れる「速度」を管理するコンポーネント。
//
// MovementComponent(入力ドリブンの移動、プル型: 毎フレーム
// 「どちらに動きたいか」を問い合わせる)とは役割が異なり、
// こちらは「向こうから飛んでくる力を、時間をかけて減衰させる」
// プッシュ型の移動を担当する。
//
// 競合の解消はこちら側からは行わない。MovementComponent/
// TweenMoveComponentが、こちらのIsMoving()を見て「外力が
// 働いている間は自分の位置更新を一時的に譲る」形にしている
// (詳細は各コンポーネントのUpdate()を参照)。
// ============================================================
class VelocityComponent : public ComponentBase {
public:
	// dampingPerSecond: 1秒間で速度がこの割合まで落ちる減衰係数。
	// 例えば0.05なら、1秒後には初速の5%まで減衰する。
	// 1.0に近いほど滑る(氷の上のような)感触、0に近いほど
	// すぐ止まる(重く粘性が高い)感触になる。
	explicit VelocityComponent(GameObject* owner, float dampingPerSecond = 0.05f)
		: ComponentBase(owner), dampingPerSecond_(dampingPerSecond) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ == nullptr) {
			std::printf(
				"[VelocityComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr) return;
		if (!IsMoving()) return;

		transform_->Translate(velocity_ * deltaTime);

		// 摩擦減衰
		const float decay = std::pow(dampingPerSecond_, deltaTime);
		velocity_.x *= decay;
		velocity_.y *= decay;
		velocity_.z *= decay;

		// 十分小さくなったら0に切り詰め、以降の無駄な計算とIsMoving()の
		// チラつきを防ぐ
		if (velocity_.LengthSquared() < kStopThresholdSq) {
			velocity_ = Math::Vector3::Zero;
		}
	}

	// 瞬間的な力を加える(ノックバック、パリィ反動など)。
	// 既存の速度に加算するため、連続でヒットすると勢いが増す。
	void AddImpulse(const Math::Vector3& impulse) { velocity_ += impulse; }

	// 現在の速度を問答無用で上書きしたい場合に使う
	void SetVelocity(const Math::Vector3& velocity) { velocity_ = velocity; }

	const Math::Vector3& GetVelocity() const { return velocity_; }

	// 他のコンポーネント(Movement/TweenMove)が
	// 「今は位置を譲るべきか」を判定するために問い合わせる。
	bool IsMoving() const { return velocity_.LengthSquared() > kStopThresholdSq; }

	void SetDampingPerSecond(float damping) { dampingPerSecond_ = damping; }
	float GetDampingPerSecond() const { return dampingPerSecond_; }

private:
	static constexpr float kStopThresholdSq = 0.0001f;

	TransformComponent* transform_ = nullptr;
	Math::Vector3 velocity_ = Math::Vector3::Zero;
	float dampingPerSecond_;
};
