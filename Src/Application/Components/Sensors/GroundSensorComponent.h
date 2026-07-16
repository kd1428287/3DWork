#pragma once
#include <cstdio>

#include "../Collision/ColliderComponent.h"
#include "../Transform/TransformComponent.h"
#include "../../Core/GameObject.h"
#include "../../Core/SceneContext.h"

// ============================================================
// 足元にレイを飛ばして「今、地面(ColliderLayer::Ground)に
// 接地しているか」だけを判定し、状態として保持するコンポーネント。
//
// 判定ロジック自体はRaycastSystem::RaycastClosest()にそのまま委譲し、
// このクラスは「いつ・どこから・どのレイヤーに向けて撃つか」の
// パラメータ管理と、結果の保持(IsGrounded/GetGroundNormal等)だけに
// 専念する。他コンポーネントの状態を書き換える処理は一切持たない
// (以前はここで着地時にVelocityComponent::SetContinuousVelocity()を
// 呼んでいたが、それは「センサー(読み取るだけ)」の役割を超えて
// いたため、GravityComponent側(積んだ本人)に移した)。
// 「移動できるかどうか」「ジャンプできるかどうか」「着地時に何を
// リセットするか」といった上位の判断はここでは行わず、他の
// コンポーネント側がIsGrounded()/JustLanded()を見て判断すること
// (MovementComponentがVelocityComponentの結果に関知しないのと
// 同じ構造)。
//
// --- 呼び出しタイミングについて(重要) ----------------------------
// Update()ではなくPostUpdate()でレイを撃つこと。理由:
//   ColliderRegistry::Refresh()は「ObjectManager::Update()の後」に
//   呼ばれる想定であり、CollisionSystem::Update()による押し返し
//   (位置補正)もその後に確定する。Update()の時点ではまだ
//   「このフレームの最終的な位置関係」が定まっていないため、
//   接地判定が1フレーム分ズレる。
//   Scene側は必ず以下の順序でPostUpdate()を呼ぶこと:
//     1. objectManager.Update(dt)
//     2. colliderRegistry.Refresh(objectManager)
//     3. collisionSystem.Update(colliderRegistry)   ← 押し返し確定
//     4. objectManager.PostUpdate(dt)               ← ここでこのクラスが動く
//   この順序はコンパイラでは強制されない(ObjectManager/GameObjectは
//   ColliderRegistry/CollisionSystemの存在を知らないため)。順序を
//   間違えると、CollisionMath::RayVsAABBが「レイの発射点がAABB内部
//   から始まるケース」を非対応としている都合上、めり込んだ状態から
//   撃ったレイが「当たらない」と誤判定される場合がある。
//
// --- 既知の制約 ---------------------------------------------------
//   - レイ1本のみでの判定。キャラの片足だけが崖から出ているような
//     状況では判定が不安定になりうる(複数点判定が必要ならfootOffsetを
//     基準に複数レイを撃つ形に拡張すること)。
//   - 歩ける斜面かどうかの角度判定(Slope Limit)は持たない。
//     GetGroundNormal()を呼び出し側が見て判断すること。
//   - コヨーテタイム(接地を離れた直後の猶予)は持たない。必要なら
//     JustLeftGround()のタイミングで呼び出し側がタイマーを開始すること。
//   - 着地/離地のイベントをEventBus経由では発行しない。ポーリングの
//     JustLanded()/JustLeftGround()で代用する(必要になったら
//     CollisionEnterEvent等と同様のイベント型を足す形で拡張する)。
// ============================================================
class GroundSensorComponent : public ComponentBase {
public:
	explicit GroundSensorComponent(GameObject* owner) : ComponentBase(owner) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		if (transform_ == nullptr) {
			std::printf(
				"[GroundSensorComponent] warning: %s has no TransformComponent\n",
				GetOwner()->GetName().c_str());
		}
	}

	void PostUpdate(float /*deltaTime*/) override {
		if (transform_ == nullptr) return;

		// このフレームのColliderRegistryはSceneContext経由で参照する。
		// まだ一度もRefresh()されていない(起動直後の1フレーム目など)場合は
		// nullptrのままなので、判定をスキップして前回の状態を維持する。
		const SceneContext* context = GetOwner()->GetContext();
		const ColliderRegistry* registry = context ? context->colliderRegistry : nullptr;
		if (registry == nullptr) return;

		const Math::Vector3 origin = transform_->GetPosition() + Math::Vector3(0.0f, -footOffset, 0.0f);

		RaycastSystem::Hit hit;
		const bool found = RaycastSystem::RaycastClosest(
			*registry, origin, Math::Vector3::Down, checkDistance,
			ColliderLayer::Ground, hit);

		wasGroundedLastQuery_ = isGrounded_;

		isGrounded_ = found;
		groundNormal_ = found ? hit.result.hitNormal : Math::Vector3::Up;
		groundObject_ = found ? hit.object : nullptr;

		// 他コンポーネントの状態を書き換える処理はここには置かない。
		// 「着地時にcontinuousVelocity_をどうするか」はGravityComponent
		// (積んだ本人)が、このクラスのJustLanded()/IsGrounded()を
		// 読みに来て自分で処理する(GravityComponent::Update()参照)。
	}

	// --- 参照 ----------------------------------------------------

	bool IsGrounded() const { return isGrounded_; }

	// 今フレームで新たに接地した(直前フレームは非接地だった)
	bool JustLanded() const { return isGrounded_ && !wasGroundedLastQuery_; }

	// 今フレームで新たに接地を離れた(直前フレームは接地していた)
	bool JustLeftGround() const { return !isGrounded_ && wasGroundedLastQuery_; }

	// 接地している地面の法線(非接地時はMath::Vector3::Upを返す)
	const Math::Vector3& GetGroundNormal() const { return groundNormal_; }

	// 接地している相手のGameObject(非接地時はnullptr)
	GameObject* GetGroundObject() const { return groundObject_; }

	// --- パラメータ ------------------------------------------------

	// TransformComponentの位置(通常はキャラの中心と想定)から、
	// どれだけ下に足裏があるか。
	float footOffset = 0.9f;

	// 足裏からどれだけ下まで地面を探すか。
	// 0に近すぎると段差や坂でわずかに浮いた瞬間に非接地判定されてしまい、
	// 大きすぎると宙に浮いていても接地扱いになるため、キャラの
	// 移動速度・段差の高さに応じて調整すること。
	float checkDistance = 0.15f;

private:
	TransformComponent* transform_ = nullptr;

	bool isGrounded_ = false;
	bool wasGroundedLastQuery_ = false;
	Math::Vector3 groundNormal_ = Math::Vector3::Up;
	GameObject* groundObject_ = nullptr;
};