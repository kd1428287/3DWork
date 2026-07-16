#pragma once
#include <cstdio>

#include "../Movement/VelocityComponent.h"
#include "../Sensors/GroundSensorComponent.h" 
// ============================================================
// 重力を適用するコンポーネント。
//
// 単体では何もしない(実際に位置を動かすのはVelocityComponentの役目)。
// 毎フレーム、重力加速度をVelocityComponent::AddContinuousVelocity()へ
// 注ぎ込むだけの薄いコンポーネント。
//
// 独立コンポーネントにしている理由:
//   - RequestAddComponent/RequestRemoveComponentで「一時的に無重力にする」
//     演出をアタッチ/デタッチだけで表現できる(SlowEffectComponentと同じ考え方)
//   - SetEnabled(false)でも同様にON/OFFできる(こちらは既存の仕組みそのまま)
//
// --- 接地中に加算をスキップする理由 --------------------------------
// 接地中も無条件に加算し続けると、continuousVelocity_のY成分が
// (位置自体はCollisionSystemの押し返しで沈み込まなくても)際限なく
// 大きくなり続ける。これは以下の点で無駄・危険:
//   - VelocityComponent::Update()が毎フレーム大きな下向き移動を試み、
//     CollisionSystemに押し戻されるだけの無駄な仕事を繰り返す
//   - 値が十分大きくなると、CollisionSystemが明言している
//     「連続衝突検出なし(トンネリング対策なし)」の弱点を実際に
//     踏み抜き、薄い床をすり抜ける可能性がある
// そのため接地中は加算せず、着地の瞬間に貯まった分をクリアする
// (下記Update()参照)。
//
// なお、以前はここに「MovementComponentが実行順序次第でずっと
// 動けなくなる」という理由も書いていたが、それはMovementComponentが
// VelocityComponent::IsMoving()ではなくIsImpulseActive()
// (ノックバックのみ)を見るように直したことで別途解消済み。
// 単なる重力の残留はもうMovementComponentをブロックしないため、
// この結合の理由からは外れている。
//
// GroundSensorComponentが判定する「接地しているか」は前フレームの
// PostUpdate時点の情報(1フレーム遅れる)だが、これは他の全コンポーネントが
// 前フレームの確定情報を参照する構造と変わらないため、実用上問題にならない。
//
// --- 責務の置き場所について -----------------------------------------
// 「着地時にcontinuousVelocity_をクリアする」処理は、以前は
// GroundSensorComponent側に置いていたが、それだと「読み取るだけの
// センサー」という設計原則(GroundSensorComponent.h参照)に反して
// 他コンポーネントの状態を書き換えることになっていた。積んだ本人
// (GravityComponent)が、自分でGroundSensorComponentの状態を読みに
// 行って後片付けする方が筋が良いため、ここに置いている。
// ============================================================
class GravityComponent : public ComponentBase {
public:
	// gravityAcceleration: Y軸方向の加速度(m/s^2相当)。
	// 下向きに落としたいので通常は負の値を渡す。
	explicit GravityComponent(GameObject* owner, float gravityAcceleration = -20.0f)
		: ComponentBase(owner), gravityAcceleration_(gravityAcceleration) {}

	void Start() override {
		velocity_ = GetOwner()->GetComponent<VelocityComponent>();
		if (velocity_ == nullptr) {
			std::printf(
				"[GravityComponent] warning: %s has no VelocityComponent\n",
				GetOwner()->GetName().c_str());
		}

		// 無くてもよい(任意)。無ければ従来通り常に重力を加算し続ける
		// (接地判定を持たないオブジェクト向けの後方互換)。
		groundSensor_ = GetOwner()->GetComponent<GroundSensorComponent>();
	}

	void Update(float deltaTime) override {
		if (velocity_ == nullptr) return;

		if (groundSensor_ != nullptr) {
			// 着地した瞬間、落下中に自分(GravityComponent)が積んだ
			// 下向きのcontinuousVelocity_を片付ける。
			// Y成分だけをクリアしているのは、将来的に動く床などが
			// continuousVelocity_の水平成分を使うようになった場合に、
			// それを巻き添えで消さないため。
			if (groundSensor_->JustLanded()) {
				Math::Vector3 continuousVelocity = velocity_->GetContinuousVelocity();
				continuousVelocity.y = 0.0f;
				velocity_->SetContinuousVelocity(continuousVelocity);
			}

			// 接地中は加算しない(理由はクラスコメント参照)。
			if (groundSensor_->IsGrounded()) {
				return;
			}
		}

		// continuousVelocity_(減衰しないチャンネル)に積み続ける。
		// impulseVelocity_(ノックバック等、摩擦で減衰する方)とは
		// 別チャンネルなので、ノックバックの減衰係数に引きずられず
		// 素直に加速し続ける。
		velocity_->AddContinuousVelocity({ 0.0f, gravityAcceleration_ * deltaTime, 0.0f });
	}

	void SetGravityAcceleration(float gravityAcceleration) { gravityAcceleration_ = gravityAcceleration; }
	float GetGravityAcceleration() const { return gravityAcceleration_; }

private:
	VelocityComponent* velocity_ = nullptr;
	GroundSensorComponent* groundSensor_ = nullptr; // 無くてもよい(任意)
	float gravityAcceleration_;
};