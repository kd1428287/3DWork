#pragma once

#include "MovementComponent.h"
#include "VelocityComponent.h"
#include "TweenMoveComponent.h"

// ============================================================
// MotionComposerComponent
//
// 「移動処理の解決」を担う唯一の窓口。内力(MovementComponent)・
// 外力(VelocityComponent)・演出移動(TweenMoveComponent)を合成して
// Transformの位置を確定させることに加え、当たり判定システム
// (CollisionResolver)から渡される衝突補正を受け取り、位置と外力の
// 双方に反映する。
//
// 各コンポーネントが個別にTransformを書き換えると、どのUpdateが先に
// 呼ばれるかで結果が変わる(componentOrder_はAddComponentの記述順に
// 過ぎない)。そこで各コンポーネントからは値を「読み取る」だけにし、
// 合成と適用をここ1箇所に集約することで、呼び出し順への依存を消している。
// 同じ理由で、各コンポーネントの進行(FetchDesiredVelocity / Advance /
// DampingUpdate)もこのコンポーネントが明示的に駆動する。
//
// 優先順位:
//   1. Tweenが動作中     → Tweenの絶対位置をそのまま採用(内力・外力は無視)
//   2. それ以外          → 内力 + 外力を合成して移動
//
// 位置の確定はUpdate()で行う。PostUpdate()ではなくUpdate()なのは、
// GroundSensorComponentがPostUpdate()で接地判定のレイを飛ばしているため。
// 同じフェーズで位置を確定すると、両者の呼び出し順で接地判定が
// 1フレームずれる(そしてその順序はFactoryでのAddComponentの記述順に
// 左右される)。「Updateで動かし、PostUpdateでその結果を観測する」
// という分離を保つ。
//
// 衝突補正(ApplyCollisionCorrection)は、GameObjectのライフサイクル
// フック(Update/PostUpdate)の外、CollisionSystem::Update()の解決
// フェーズから呼ばれる。フレーム内の順序は
//   ObjectManager::Update()(内力・外力の合成、暫定位置の確定)
//   → CollisionSystem::Update()(検出 → CollisionResolverが集約
//     → ApplyCollisionCorrection())
//   → ObjectManager::PostUpdate()(GroundSensor等が最終位置を観測)
// となるため、ここでもUpdate/PostUpdateとの順序競合は起きない。
// ============================================================
class MotionComposerComponent : public ComponentBase
{
public:
	explicit MotionComposerComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movement_ = GetOwner()->GetComponent<MovementComponent>();
		velocity_ = GetOwner()->GetComponent<VelocityComponent>();
		tween_ = GetOwner()->GetComponent<TweenMoveComponent>();

		if (transform_ == nullptr) {
			std::printf(
				"[MotionComposerComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	void Update(float deltaTime) override
	{
		if (!transform_) return;

		const float clampedDeltaTime = std::min(deltaTime, kMaxDeltaTime);

		// --- 各コンポーネントの進行を駆動する -------------------------
		if (movement_) movement_->FetchDesiredVelocity();
		if (tween_)    tween_->Advance(clampedDeltaTime);

		// --- 1. Tweenが動作中なら、絶対位置で上書きして終わり ----------
		// Tweenは「速度」ではなく「位置」を決めるものなので、内力・外力と
		// 足し合わせることができない。動作中は他に優越する。
		if (tween_ && tween_->IsActive())
		{
			transform_->SetPosition(tween_->GetCurrentPosition());

			// 内力・外力は適用しないが、外力の減衰だけは進めておく。
			// そうしないとTween終了の瞬間に、Tween開始前のノックバックが
			// 減衰されないまま復活して吹き飛ぶ。
			if (velocity_) velocity_->DampingUpdate(clampedDeltaTime);

			// Tween中は「速度」という概念が無いため、参照側には
			// 動いていない扱いで見せる。
			lastComposedVelocity_ = Math::Vector3::Zero;
			return;
		}

		// --- 2. 内力 + 外力の合成 --------------------------------------
		const Math::Vector3 externalVelocity = velocity_ ? velocity_->GetVelocity() : Math::Vector3::Zero;
		const Math::Vector3 internalVelocity = movement_ ? movement_->GetVelocity() : Math::Vector3::Zero;

		Math::Vector3 composedVelocity = externalVelocity + internalVelocity;

		// 水平と垂直を分けてクランプする。
		// ひとまとめに正規化すると、落下速度(重力による垂直成分)が
		// 水平方向の速度予算を食ってしまい、「高所から落ちている間だけ
		// 空中制御が効かなくなる」という挙動になるため。
		Math::Vector3 horizontal{ composedVelocity.x, 0.0f, composedVelocity.z };
		if (horizontal.LengthSquared() > kMaxHorizontalSpeed * kMaxHorizontalSpeed)
		{
			horizontal.Normalize();
			horizontal *= kMaxHorizontalSpeed;
			composedVelocity.x = horizontal.x;
			composedVelocity.z = horizontal.z;
		}

		composedVelocity.y = std::clamp(composedVelocity.y, -kMaxVerticalSpeed, kMaxVerticalSpeed);

		transform_->SetPosition(transform_->GetPosition() + composedVelocity * clampedDeltaTime);
		lastComposedVelocity_ = composedVelocity;

		// --- 3. 外力の減衰 ---------------------------------------------
		// 適用した「後」に減衰させる。先に減衰させると、ノックバックが
		// 発生した最初の1フレームから既に減衰後の値が使われてしまう。
		if (velocity_) velocity_->DampingUpdate(clampedDeltaTime);
	}

	// CollisionResolverが、このオブジェクト分の衝突補正がすべて
	// 出揃った後にフレームに1回だけ呼ぶ。位置に補正を適用しつつ、
	// 補正方向と逆(=めり込みを助長する向き)を向いている外力の成分を
	// 打ち消す。内力(MovementComponent)は毎フレーム入力から再計算
	// されるため、訂正の対象にしない。
	void ApplyCollisionCorrection(const Math::Vector3& delta)
	{
		if (!transform_) return;
		if (delta.LengthSquared() <= 1e-10f) return;

		transform_->SetPosition(transform_->GetPosition() + delta);

		if (velocity_) {
			Math::Vector3 normal = delta;
			normal.Normalize();

			Math::Vector3 continuous = velocity_->GetContinuousVelocity();
			const float alongNormal = continuous.Dot(normal);
			if (alongNormal < 0.0f) {
				continuous -= normal * alongNormal;
				velocity_->SetContinuousVelocity(continuous);
			}
		}
	}

	// 他システム(アニメーションのブレンド、デバッグ表示等)が
	// 「今フレーム実際に合成された速度」を読みたい場合用。
	// Tween動作中はゼロを返す(Tweenには速度という概念が無いため)。
	Math::Vector3 GetComposedVelocity() const { return lastComposedVelocity_; }

private:
	TransformComponent* transform_ = nullptr;
	MovementComponent* movement_ = nullptr;   // 無くてもよい(任意)
	VelocityComponent* velocity_ = nullptr;   // 無くてもよい(任意)
	TweenMoveComponent* tween_ = nullptr;     // 無くてもよい(任意)

	// GetComposedVelocity()で外部に公開する、直近Update()での合成速度。
	Math::Vector3 lastComposedVelocity_ = Math::Vector3::Zero;

	static constexpr float kMaxDeltaTime = 1.0f / 30.0f;

	// 水平方向の最大速度
	static constexpr float kMaxHorizontalSpeed = 25.0f;

	// 垂直方向の最大速度
	static constexpr float kMaxVerticalSpeed = 30.0f;
};