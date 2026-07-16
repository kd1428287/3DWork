#pragma once
#include <cstdio>
#include <cmath>

#include "../Transform/TransformComponent.h"

// ============================================================
// 外力によって駆動される「速度」を管理するコンポーネント。
//
// MovementComponent(入力ドリブンの移動、プル型: 毎フレーム
// 「どちらに動きたいか」を問い合わせる)とは役割が異なり、
// こちらは「向こうから飛んでくる力を受けて動く」プッシュ型の
// 移動を担当する。
//
// 内部の速度は性質の異なる2チャンネルに分けて持つ。
//   - impulseVelocity_    : 瞬間的な力(ノックバック、パリィ反動など)。
//                           毎フレーム摩擦(dampingPerSecond_)で減衰する。
//   - continuousVelocity_ : 継続的な力(重力など)。GravityComponent等が
//                           毎フレーム加算し続ける想定で、こちらは
//                           減衰しない(着地/衝突などで明示的に
//                           ClearContinuousVelocity()されるまで蓄積される)。
// 位置への反映は両者を合算した値を使う。分けているのは、重力の
// 落下速度がノックバックの減衰係数(dampingPerSecond_)に引きずられて
// 終端速度化してしまう、という意図しない結合を避けるため。
//
// 競合の解消はこちら側からは行わない。MovementComponent/
// TweenMoveComponentが、こちらのIsImpulseActive()を見て
// 「ノックバック等の外力で押し出されている間は自分の位置更新を
// 一時的に譲る」形にしている(詳細は各コンポーネントのUpdate()を参照)。
// 単なる重力による落下(continuousVelocity_)はここでの「譲るべき
// 状態」には含めない点に注意(IsMoving()とIsImpulseActive()の
// 使い分けは各メソッドのコメント参照)。
// ============================================================
class VelocityComponent : public ComponentBase {
public:
	// dampingPerSecond: impulseVelocity_が1秒間でこの割合まで落ちる減衰係数。
	// 例えば0.05なら、1秒後には初速の5%まで減衰する。
	// 1.0に近いほど滑る(氷の上のような)感触、0に近いほど
	// すぐ止まる(重く粘性が高い)感触になる。
	// continuousVelocity_(重力等)には適用されない。
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

		const Math::Vector3 totalVelocity = impulseVelocity_ + continuousVelocity_;
		if (totalVelocity.LengthSquared() > kStopThresholdSq) {
			transform_->Translate(totalVelocity * deltaTime);
		}

		// 摩擦減衰。impulseVelocity_にだけ適用する
		// (continuousVelocity_は重力のように減衰させたくない力のため対象外)。
		const float decay = std::pow(dampingPerSecond_, deltaTime);
		impulseVelocity_ *= decay;

		// 十分小さくなったら0に切り詰め、以降の無駄な計算とIsMoving()の
		// チラつきを防ぐ
		if (impulseVelocity_.LengthSquared() < kStopThresholdSq) {
			impulseVelocity_ = Math::Vector3::Zero;
		}
	}

	// 瞬間的な力を加える(ノックバック、パリィ反動など)。
	// 既存のimpulseVelocity_に加算するため、連続でヒットすると勢いが増す。
	// 摩擦で自然に減衰していく。
	void AddImpulse(const Math::Vector3& impulse) { impulseVelocity_ += impulse; }

	// 継続的な力を加える(重力など)。GravityComponentのように
	// 毎フレーム呼ばれ続けることを想定している。摩擦で減衰しないため、
	// 着地・衝突などで止めたい場合はClearContinuousVelocity()を明示的に呼ぶこと
	// (現状、地面との衝突応答はCollisionSystem側に未実装。要注意)。
	void AddContinuousVelocity(const Math::Vector3& delta) { continuousVelocity_ += delta; }

	// continuousVelocity_を問答無用で上書きしたい場合に使う
	void SetContinuousVelocity(const Math::Vector3& velocity) { continuousVelocity_ = velocity; }
	const Math::Vector3& GetContinuousVelocity() const { return continuousVelocity_; }

	// 着地時などにcontinuousVelocity_だけをリセットしたい場合に使う
	// (impulseVelocity_によるノックバックの勢いはそのまま残したい、
	//  というケースが多いためimpulseVelocity_は対象に含めない)。
	void ClearContinuousVelocity() { continuousVelocity_ = Math::Vector3::Zero; }

	// impulseVelocity_を問答無用で上書きしたい場合に使う
	void SetImpulseVelocity(const Math::Vector3& velocity) { impulseVelocity_ = velocity; }
	const Math::Vector3& GetImpulseVelocity() const { return impulseVelocity_; }

	// 合算後の速度(位置に実際反映される値)
	Math::Vector3 GetVelocity() const { return impulseVelocity_ + continuousVelocity_; }

	// 合算後の速度で実際に動いているかどうか。
	// アニメーションの「移動中フラグ」など、外力の種類を問わず
	// 「今物理的に動いているか」だけを知りたい用途向け。
	// MovementComponentが「入力移動を譲るべきか」の判定には
	// これではなくIsImpulseActive()を使うこと(下記参照)。
	bool IsMoving() const {
		return (impulseVelocity_ + continuousVelocity_).LengthSquared() > kStopThresholdSq;
	}

	// 他のコンポーネント(Movement/TweenMove)が
	// 「今は入力移動を一時的に譲るべきか」を判定するために問い合わせる。
	//
	// impulseVelocity_(ノックバック等)だけを見て、continuousVelocity_
	// (重力など)は含めない。単に重力で落下しているだけの状態は、
	// MovementComponentによる水平方向の入力移動と本質的に競合しない
	// (落下は主にY軸、入力移動は主にXZ平面であることが多く、そもそも
	//  軸が違う)ため、ここで「譲るべき状態」には含めない。一方
	// ノックバックは「今まさに外力によって強制的に押し出されている」
	// 状態であり、この間に入力移動が直接Transformを書き換えると
	// ノックバックの勢いを打ち消してしまうため、こちらは引き続き
	// 譲るべき対象とする。
	bool IsImpulseActive() const {
		return impulseVelocity_.LengthSquared() > kStopThresholdSq;
	}

	void SetDampingPerSecond(float damping) { dampingPerSecond_ = damping; }
	float GetDampingPerSecond() const { return dampingPerSecond_; }

private:
	static constexpr float kStopThresholdSq = 0.0001f;

	TransformComponent* transform_ = nullptr;
	Math::Vector3 impulseVelocity_ = Math::Vector3::Zero;
	Math::Vector3 continuousVelocity_ = Math::Vector3::Zero;
	float dampingPerSecond_;
};