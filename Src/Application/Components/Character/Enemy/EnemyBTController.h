#pragma once
#include <memory>
#include "../BehaviorTree/IBTNode.h"
#include "../BehaviorTree/BTNodeStatus.h"
#include "../BehaviorTree/BTComposite.h"
#include "../BehaviorTree/BTCondition.h"
#include "EnemyActionAttack.h"
#include "EnemyStatusController.h"
#include "../Player/PlayerStatusController.h"
#include "../../Transform/TransformComponent.h"

// ============================================================
// 敵の「今何をすべきか」の意思決定だけを担当するController。
// 実際の攻撃モーション進行(Windup/Active/Recovery)・被弾リアクション
// (Knockback/ParryStun/Dead)は全てEnemyStatusController(Stateパターン、
// 実体はBrute/BossStatusController)側が担当し、こちらは一切手を出さない。
//
// 【EnemyActionIdleは使わない】
// パトロール(StateWalkRight/StateWalkLeft)はEnemyStatusController内部で
// 常に自律的にループしている。BTが「射程内なので攻撃する」と判断しない
// 限りは何もせず、その間は内部Stateが勝手にパトロールを続ける。つまり
// Enemy側には「BTが明示的にIdleを選ぶ」場面自体が存在しないため、
// Player寄りの初期検証で使っていたEnemyActionIdleノードはここでは
// 使用しない(CanAct()に相当するAPIも実際のEnemyStatusControllerには
// 無い)。
//
// 【重要: EnemyStatusController*はGetComponent()で自己解決しない】
// GameObject::GetComponent<T>()はTの具体型そのものでしか検索できない。
// 実際の敵はBruteStatusController/BossStatusControllerという派生クラス
// として生成される(EnemyFactory::CreateStatusController()参照)ため、
// GetOwner()->GetComponent<EnemyStatusController>()では見つけられない。
// 生成元(EnemyFactory::AttachBehaviorTree())からコンストラクタで
// 直接渡してもらう形で回避している。
//
// 【重要: コンポーネントの追加順序】
// EnemyFactory::BuildEnemy()側で、EnemyStatusController(CreateStatusController)
// を先に、EnemyBTController(AttachBehaviorTree)を後から追加すること。
// ============================================================
class EnemyBTController : public ComponentBase
{
public:
	explicit EnemyBTController(GameObject* owner, EnemyStatusController* statusController)
		: ComponentBase(owner), statusController_(statusController) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		auto attackSequence = std::make_unique<BTSequence<EnemyBTController>>();
		attackSequence->AddChild(std::make_unique<BTCondition<EnemyBTController>>(
			[](EnemyBTController* c) { return c->IsPlayerInAttackRange(); }));
		attackSequence->AddChild(std::make_unique<EnemyActionAttack>());

		// 現状は「射程内なら攻撃」の1候補のみ。射程外の間はこのSequenceが
		// Failureを返すだけで、BT側は何もしない(パトロールは内部Stateに
		// 任せきりでよい)。Selectorで包んでおくのは、将来Chase等の候補を
		// 追加できる余地を残すため。
		auto selector = std::make_unique<BTSelector<EnemyBTController>>();
		selector->AddChild(std::move(attackSequence));

		root_ = std::move(selector);
	}

	void Update(float deltaTime) override
	{
		if (root_ != nullptr) root_->Tick(this, deltaTime);
	}

	// --- Action/Conditionノードから呼ばれるAPI ---------------------------
	EnemyStatusController* GetStatusController() const { return statusController_; }

	// プレイヤーとの距離が、この敵の間合い(EnemyStatusData::attackRange)
	// 以内かどうか。BTCondition<EnemyBTController>から呼ばれる。
	bool IsPlayerInAttackRange() const
	{
		if (statusController_ == nullptr || transform_ == nullptr) return false;

		TransformComponent* playerTransform = FindPlayerTransform();
		if (playerTransform == nullptr) return false;

		const float range = statusController_->GetAttackRange();
		const float distSq = (playerTransform->GetPosition() - transform_->GetPosition()).LengthSquared();
		return distSq <= range * range;
	}

	// EnemyActionAttackが攻撃開始時の向き直し(EnemyStatusController::
	// TryStartAttack()のtargetPosition引数)に使う。プレイヤーが見つからない
	// 場合は自分の現在位置を返す(=向きを変えない)。
	Math::Vector3 GetPlayerPositionOrSelf() const
	{
		if (TransformComponent* playerTransform = FindPlayerTransform()) {
			return playerTransform->GetPosition();
		}
		return transform_ != nullptr ? transform_->GetPosition() : Math::Vector3::Zero;
	}

private:
	// シーン上のプレイヤーを毎回検索する。ObjectManager::FindComponents<T>()は
	// 線形走査だが、敵の数・呼び出し頻度(1体あたり毎フレーム1回)の
	// ポートフォリオ規模では許容範囲(PlayerLockOnComponentが同じ方式を
	// 採用しているのと同じ判断)。プレイヤーが複数居る想定は無いため、
	// 最初に見つかった1体を使う。
	TransformComponent* FindPlayerTransform() const
	{
		SceneContext* context = GetOwner()->GetContext();
		if (context == nullptr || context->objectManager == nullptr) return nullptr;

		for (PlayerStatusController* player : context->objectManager->FindComponents<PlayerStatusController>()) {
			if (TransformComponent* t = player->GetOwner()->GetComponent<TransformComponent>()) {
				return t;
			}
		}
		return nullptr;
	}

	EnemyStatusController* statusController_ = nullptr;
	TransformComponent* transform_ = nullptr;

	std::unique_ptr<IBTNode<EnemyBTController>> root_;
};
