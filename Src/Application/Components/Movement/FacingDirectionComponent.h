#pragma once

#include "../Transform/TransformComponent.h"
#include "VelocityComponent.h"

// ============================================================
// FacingDirectionComponent
//
// 「前フレームからTransformの位置がどちらへ動いたか」を毎フレーム
// 観測し、その水平方向(Y成分を無視した進行方向)へ、Y軸まわりの
// Yaw回転のみをSlerpで滑らかに追従させる。
//
// MovementComponent(入力駆動)/TweenMoveComponent(演出移動)のどちらで
// 実際にTransformを動かしたかには関知しない(位置差分だけを見る設計)。
//
// ただしVelocityComponent(外力駆動。ノックバック等)によるimpulse移動
// だけは例外で、IsImpulseActive()==trueの間は向きの更新自体を一時停止
// する。ノックバックで吹っ飛ばされている方向へ向き直ってしまうと、
// 「怯み中に攻撃を受けた方向を向いたまま」という自然な演出にならない
// ため(MovementComponent/TweenMoveComponentが同じ理由でIsImpulseActive()
// を見て自分の位置更新を一時的に譲っているのと同じ考え方。VelocityComponent
// が無いGameObjectにアタッチしても問題なく動く。任意添付)。
// ============================================================
class FacingDirectionComponent : public ComponentBase {
public:
	// rotationSpeed: 向きの追従の速さ(大きいほど素早く向き直る。
	//   毎フレームのSlerpのt値は rotationSpeed * deltaTime で決まる)。
	// moveThreshold: 1フレームあたりの移動量がこれ未満なら「動いていない」
	//   とみなし、回転を更新しない(停止直前の僅かな位置ノイズで
	//   向きがガタつくのを防ぐ)。
	explicit FacingDirectionComponent(GameObject* owner,
		float rotationSpeed = 10.0f, float moveThreshold = 0.001f)
		: ComponentBase(owner), rotationSpeed_(rotationSpeed), moveThreshold_(moveThreshold) {
	}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ != nullptr) {
			lastPosition_ = transform_->GetPosition();
			hasLastPosition_ = true;
		}

		// 無くてもよい(任意)。存在する場合のみノックバック中の向き固定に使う。
		velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr) return;

		const Math::Vector3 currentPosition = transform_->GetPosition();

		if (!hasLastPosition_) {
			// 初回フレームは前の位置が無く差分が取れないため、記録だけして終わる。
			lastPosition_ = currentPosition;
			hasLastPosition_ = true;
			return;
		}

		// ノックバック中(外力で強制的に押し出されている間)は向きを固定する。
		// 「今どちらへ飛ばされているか」ではなく「攻撃を受けた時点の向き」を
		// 保ちたいため、この間はlastPosition_の更新も含めて丸ごとスキップする
		// (位置差分だけ蓄積させて後で反映する、という中途半端な状態にしないため。
		//  ノックバックが終わった直後の1フレームで大きな差分が出て急に
		//  振り向く、という事故を避ける)。
		if (velocityComponent_ != nullptr && velocityComponent_->IsImpulseActive()) {
			lastPosition_ = currentPosition;
			return;
		}

		Math::Vector3 delta = currentPosition - lastPosition_;
		lastPosition_ = currentPosition;

		// 上下移動(ジャンプ・落下・段差の乗り上げ等)は向きの判定に使わない。
		delta.y = 0.0f;

		if (delta.LengthSquared() < moveThreshold_ * moveThreshold_) {
			// ほぼ動いていない場合は現在の向きを維持する
			// (立ち止まった瞬間に正面へリセットされるような不自然さを防ぐ)。
			return;
		}

		delta.Normalize();
		const Math::Quaternion targetRotation = LookRotationYawOnly(delta);

		// rotationSpeed_ * deltaTimeをそのままtに使うと、フレームレートが
		// 極端に低い場合に1を超えうるためclampしておく(Slerpの定義域外を
		// 渡さないため)。
		const float t = std::clamp(rotationSpeed_ * deltaTime, 0.0f, 1.0f);
		transform_->SetRotation(Math::Quaternion::Slerp(transform_->GetRotation(), targetRotation, t));
	}

	void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }
	float GetRotationSpeed() const { return rotationSpeed_; }

	void SetMoveThreshold(float threshold) { moveThreshold_ = threshold; }
	float GetMoveThreshold() const { return moveThreshold_; }

private:
	// Math::Vector3::Forward(TransformComponent::GetForward()が基準にしている
	// ローカル前方軸)から、水平方向dirへの最短回転を内積・外積だけで求める。
	// atan2ベースの角度計算を使わないのは、エンジンのForward軸が+Z/-Zの
	// どちらの規約でも(左手系/右手系のどちらでも)同じコードで正しく
	// 動くようにするため。
	static Math::Quaternion LookRotationYawOnly(const Math::Vector3& dir) {
		constexpr float kEpsilon = 1e-5f;

		const Math::Vector3 from = Math::Vector3::Forward;
		const float dot = std::clamp(from.Dot(dir), -1.0f, 1.0f);

		if (dot > 1.0f - kEpsilon) return Math::Quaternion::Identity; // 既に正面を向いている

		if (dot < -1.0f + kEpsilon) {
			// ほぼ真後ろ: 外積がゼロベクトルに近づき回転軸が定まらないため、
			// Up軸まわり180度の回転で代用する。
			const float kPi = std::acos(-1.0f);
			return Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, kPi);
		}

		Math::Vector3 axis = from.Cross(dir);
		axis.Normalize();
		const float angle = std::acos(dot);
		return Math::Quaternion::CreateFromAxisAngle(axis, angle);
	}

	TransformComponent* transform_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr; // 無くてもよい(任意)
	Math::Vector3 lastPosition_{};
	bool hasLastPosition_ = false;

	float rotationSpeed_;
	float moveThreshold_;
};