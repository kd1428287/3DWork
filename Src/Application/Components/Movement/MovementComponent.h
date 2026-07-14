#pragma once
#include <cstdio>

#include "IMovementSource.h"
#include "../Transform/TransformComponent.h"
#include "VelocityComponent.h"

// ============================================================
// サンプルコンポーネント その2: Movement
//
// 「動く」という処理自体(Transformを書き換える部分)と、
// 「どちらに動くか決める」処理(IMovementSource)を分離している。
// これにより、手動操作(入力コンポーネント)・自動操作(AIコンポーネント)を
// 同じMovementComponentでそのまま扱える。
//
// 依存コンポーネントの取得は Start() で行うのが定石
// (Awake順序に依らず、全コンポーネントの追加が終わってから解決できるため)。
//
// VelocityComponent(外力による移動)とは役割が違うため分離しており、
// 両者が同時に位置を書き換えないよう、VelocityComponentがIsMoving()==true
// の間はこちらの位置更新を一時停止する(任意添付。無くても動作する)。
// ============================================================
class MovementComponent : public ComponentBase {
public:
	explicit MovementComponent(GameObject* owner, float speed = 1.0f)
		: ComponentBase(owner), speed_(speed) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// VelocityComponentは無くてもよい(ノックバック等が不要なオブジェクト向け)。
		// 存在する場合のみ、外力が働いている間の位置更新の優先譲渡に使う。
		velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
	}

	void Update(float deltaTime) override {
		if (transform_ == nullptr || source_ == nullptr) return;

		// 外力(ノックバックなど)が働いている間は、そちらの移動を優先し、
		// 通常の入力移動は一時停止する(位置の二重書き換えを避けるため)。
		if (velocityComponent_ != nullptr && velocityComponent_->IsMoving()) return;

		const Math::Vector3 v = source_->GetDesiredVelocity();
		transform_->position.x += v.x * speed_ * deltaTime;
		transform_->position.y += v.y * speed_ * deltaTime;
		transform_->position.z += v.z * speed_ * deltaTime;
	}

	// 動きの決定方法(手動入力 / AI など)を差し替える。
	// PlayerInputComponent* や AIWanderComponent* など、
	// IMovementSourceを実装したコンポーネントを渡す。
	void SetMovementSource(IMovementSource* source) { source_ = source; }

	void SetSpeed(float speed) { speed_ = speed; }
	float GetSpeed() const { return speed_; }

private:
	float speed_;
	TransformComponent* transform_ = nullptr;
	IMovementSource* source_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr; // 無くてもよい(任意)
};