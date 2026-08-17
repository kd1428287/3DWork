#pragma once
#include "EnemyState.h"
#include "../../Movement/IMovementSource.h"
#include "../../Movement/MovementComponent.h"
#include "../../Movement/VelocityComponent.h"
#include "../../Transform/TransformComponent.h"
#include "../StateMachine/StateMachine.h"
#include "../../../Systems/Collision/CollisionSystem.h"
#include "../../Collision/AttackSourceComponent.h"

// ============================================================
// EnemyStatusController
// 基準点(Start()時点の初期位置)から左右に往復するだけの、
// 最も単純なパトロールAI。PlayerStatusControllerと同じ共通StateMachine基盤
// (StateMachine<TController, TState>)で構成している。
//
// PlayerInputComponentのような「外部から入力を注入されるIMovementSource」
// ではなく、AI自身が「今どちらに動きたいか」を決定するため、
// このコントローラ自身がIMovementSourceを実装し、Start()時点で
// MovementComponentへ直接セットする(以前のAIWanderComponentと
// 同じ考え方)。
//
// ※ IMovementSource::GetDesiredVelocity()はdeltaTime引数を取らない
//   前提で書いている(最新のPlayerInputComponent.hに合わせた)。
//   もし手元のIMovementSource.hがまだdeltaTime引数ありのままなら、
//   このファイルとMovementComponent側の呼び出しをどちらかに揃えること。
// ============================================================
class EnemyStatusController : public ComponentBase, public IMovementSource {
public:
	// patrolDistance: 基準点からどれだけ離れたら折り返すか
	explicit EnemyStatusController(GameObject* owner, float patrolDistance = 3.0f)
		: ComponentBase(owner), patrolDistance_(patrolDistance) {
	}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();

		if (transform_ != nullptr) {
			basePosition_ = transform_->GetPosition();
		}
		if (movementComponent_ != nullptr) {
			movementComponent_->SetMovementSource(this);
		}

		// HurtBoxへのCollisionEnterEventは、シーン共有バスではなく
		// このGameObject自身のローカルバスにだけ届く(CollisionSystem参照)。
		// なので「自分宛てかどうか」のチェックは不要で、shapeNameの
		// フィルタだけで足りる。
		//
		// ScopedSubscriber自体はSubscribe()を持たない(購読IDを保持して
		// デストラクタでUnsubscribe()するだけのRAIIラッパー)。実際の
		// 購読はEventBus::Subscribe<T>()側で行い、戻り値のIDをバスの
		// ポインタと一緒にScopedSubscriberへ渡して、スコープを抜けたら
		// (=このコンポーネントが破棄されたら)自動解除されるようにする。
		EventBus& localBus = GetOwner()->GetLocalEventBus();
		const SubscriptionId subscriptionId = localBus.Subscribe<CollisionSystem::CollisionEnterEvent>(
			[this](const CollisionSystem::CollisionEnterEvent& e) { OnCollisionEnter(e); });
		subscriber_ = ScopedSubscriber(&localBus, subscriptionId);

		TransitionTo(&stateWalkRight_);
	}

	void Update(float deltaTime) override {
		// 戦闘状態の更新は共通StateMachineに丸投げ
		stateMachine_.Update(this, deltaTime);
	}

	// --- Stateから参照される情報 ------------------------------------------

	const Math::Vector3& GetBasePosition() const { return basePosition_; }
	float GetPatrolDistance() const { return patrolDistance_; }

	Math::Vector3 GetCurrentPosition() const {
		return transform_ ? transform_->GetPosition() : basePosition_;
	}

	// --- 状態遷移(Stateから呼ばれる) ---------------------------------------

	void ChangeStateToWalkRight() { TransitionTo(&stateWalkRight_); }
	void ChangeStateToWalkLeft() { TransitionTo(&stateWalkLeft_); }

	// HurtBox命中時に呼ばれる。既にKnockback中でも、より強い/新しい
	// ヒットで怯みを取り直したいのでForceTransitionTo(同一Stateでも
	// Exit→Enterし直す版)を使い、内部タイマーをリセットする。
	//
	// 適用前にClampKnockbackParams()で安全域にクランプする。実際の吹っ飛び
	// 移動自体はVelocityComponent::AddImpulse()(StateKnockback::Enter経由)
	// に一本化しているため、以前ほどトンネリングは起きにくくなったが、
	// 極端な速度・下向き成分を渡した場合の安全策として引き続き残している。
	void ChangeStateToKnockback(const KnockbackParams& params) {
		stateKnockback_.SetParams(ClampKnockbackParams(params));
		stateMachine_.ForceTransitionTo(this, &stateKnockback_);
	}

	// --- StateKnockbackから呼ばれる、VelocityComponentへの橋渡し -----------
	// EnemyStatusController自身はVelocityComponentを直接知らなくても
	// 動くようにしたいStateWalkRight/Left側の設計は変えたくないため、
	// ここで薄いラッパーとして仲介する。
	void ApplyKnockbackImpulse(const Math::Vector3& impulse) {
		if (velocityComponent_ != nullptr) velocityComponent_->AddImpulse(impulse);
	}
	bool IsKnockbackImpulseActive() const {
		return velocityComponent_ != nullptr && velocityComponent_->IsImpulseActive();
	}

	// --- IMovementSourceの実装 ---------------------------------------------
	// MovementComponentから毎フレーム問い合わせられる。
	Math::Vector3 GetDesiredVelocity() override { return desiredDirection_; }

	// Stateがこのフレームの移動方向を設定するために使う。
	void SetDesiredDirection(const Math::Vector3& dir) { desiredDirection_ = dir; }

private:
	void TransitionTo(IEnemyState* nextState) {
		stateMachine_.TransitionTo(this, nextState);
	}

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr; // ノックバックの実際の移動はこちらに委譲する

	Math::Vector3 basePosition_{};
	float patrolDistance_;
	Math::Vector3 desiredDirection_{};

	// Stateインスタンス(メモリ断片化を防ぐため実体をメンバで持つ。
	// PlayerStatusControllerと同じ理由)。
	StateWalkRight stateWalkRight_;
	StateWalkLeft stateWalkLeft_;
	StateKnockback stateKnockback_;

	// 共通StateMachine基盤(PlayerStatusControllerと同じテンプレートを使い回す)
	StateMachine<EnemyStatusController, IEnemyState> stateMachine_;

	// --- 当たり判定連携 ------------------------------------------------

	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e) {
		// このコントローラが反応すべきは自分側がHurtBoxとして受けた通知だけ。
		// (同じGameObjectが足元の接地レイ用コライダー等、他の形状を
		//  持っている場合に、それらのEnterと混ざらないようにするため)
		if (e.selfShapeName != "HurtBox") return;

		AttackSourceComponent* attack = e.otherObject->GetComponent<AttackSourceComponent>();
		if (attack == nullptr) return;

		// 多段ヒット防止。同じ攻撃(=HitBoxがenabled=trueになっている間)で
		// 既にこの相手(自分)へヒット済みなら無視する。HitBoxが再度
		// enabled=trueになる(=次の攻撃が始まる)たびにPlayerStatusController::
		// SetWeaponHitBoxEnabled()側でクリアされる想定(alreadyHitの
		// クリアタイミングは攻撃側が管理する。詳細はAttackSourceComponent.h参照)。
		if (attack->alreadyHit.count(GetOwner()) > 0) return;
		attack->alreadyHit.insert(GetOwner());

		// e.hitResult.hitNormalは既に「self(=このHurtBox)を押し出す方向」に
		// 正規化済み(CollisionSystem::MakeEnterEventのflipNormal参照)なので、
		// そのままノックバック方向として使える。
		// ※現状のパトロールAI(StateWalkRight/Left)はX軸移動のみのテスト用
		//   簡易実装だが、これは移動システム側が最終的に3D対応する前提の
		//   暫定であり、当たり判定・ノックバック側をX軸限定に合わせて
		//   歪める必要は無い。hitNormalの向きをそのまま使うのが正しい。
		KnockbackParams params;
		params.direction = e.hitResult.hitNormal;
		params.power = attack->knockbackPower;
		params.minStunDuration = attack->hitStunSeconds;

		ChangeStateToKnockback(params);
	}

	ScopedSubscriber subscriber_;

	// --- ノックバックの安全域クランプ(地面すり抜け対策) -------------------
	//
	// 恒久対応(CCD)ではなく、あくまで軽量な緩和策である点に注意。
	// 「1フレームでの最大移動量」を大きく超えるような異常な速度を防ぐのが
	// 目的で、地形の厚みそのものが薄すぎる場合は根本対策にならない
	// (別途、地面コライダーに十分な厚みを持たせることも推奨)。

	// 1秒あたりのノックバック速度の上限。VelocityComponent::AddImpulse()に
	// そのまま渡す値なので、ここでのpowerの単位は「1秒あたりの移動距離」
	// とみなせる(MovementComponentのspeed_のような追加の倍率はかからない)。
	static constexpr float kMaxKnockbackSpeed = 15.0f;

	// ノックバック方向のY成分(下向き)の下限。-1.0(完全に真下)に近いほど
	// 高速で地面へ潜り込みやすくなるため、真下方向をある程度までに制限する
	// (真横〜斜め下までは許容し、「真下へ垂直落下するような」極端な
	//  ケースだけ緩和する)。
	static constexpr float kMinKnockbackDirectionY = -0.5f;

	static KnockbackParams ClampKnockbackParams(KnockbackParams params) {
		params.direction = ClampKnockbackDirection(params.direction);
		params.power = std::min(params.power, kMaxKnockbackSpeed);
		return params;
	}

	// direction.yを単純に上書きしてからNormalize()すると、水平成分が
	// 小さいベクトル(ほぼ真下/真上向き)ほど正規化の過程でY成分が元の
	// 値近くまで引き戻されてしまい、クランプが実質無効化される
	// (水平成分以外に正規化がすがる先が無いため)。
	// 例: {0.1, -0.99, 0.1}のyを-0.5に置き換えて正規化すると、
	//     長さを1に戻す過程でyは-0.96近くまで戻ってしまう。
	//
	// 正しくは「yをクランプした上で、水平成分の長さがsqrt(1-y^2)になる
	// よう水平方向だけ再スケールする」必要がある。これなら仕上がりが
	// 必ず単位ベクトルになり、yも狙った値のまま保たれる。
	static Math::Vector3 ClampKnockbackDirection(const Math::Vector3& direction) {
		const float clampedY = std::max(direction.y, kMinKnockbackDirectionY);

		Math::Vector3 horizontal(direction.x, 0.0f, direction.z);
		const float horizontalLenSq = horizontal.LengthSquared();

		constexpr float kEpsilon = 1e-4f;
		if (horizontalLenSq < kEpsilon) {
			// 水平成分がほぼゼロ(真上/真下からほぼ垂直に当たったケース)。
			// 向きを再スケールする基準が無いため、既定の水平方向
			// (Forward軸)にフォールバックする。
			horizontal = Math::Vector3::Forward;
		}
		else {
			horizontal /= std::sqrt(horizontalLenSq); // 水平方向だけ正規化
		}

		// clampedY^2 + horizontalLen^2 = 1 になるよう水平成分の長さを
		// 再スケールする。これで仕上がりが必ず単位ベクトルになる。
		const float horizontalTargetLen = std::sqrt(std::max(0.0f, 1.0f - clampedY * clampedY));
		horizontal *= horizontalTargetLen;

		return Math::Vector3(horizontal.x, clampedY, horizontal.z);
	}
};