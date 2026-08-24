#pragma once
#include <vector>
#include "EnemyState.h"
#include "../../Movement/IMovementSource.h"
#include "../../Movement/MovementComponent.h"
#include "../../Movement/VelocityComponent.h"
#include "../../Transform/TransformComponent.h"
#include "../StateMachine/StateMachine.h"
#include "../../../Systems/Collision/CollisionSystem.h"
#include "../../Collision/AttackSourceComponent.h"
#include "../../Collision/ColliderComponent.h"
#include "../Data/PostureComponent.h"
#include "../Data/HealthComponent.h"
#include "../../../Core/ObjectManager.h"

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
//
// --- ファイル構成について ------------------------------------------
// PlayerStatusController.h/.cppと同じ方針で分離している: 入力処理や
// 計算量の多いロジック(OnCollisionEnter/OnDied/ノックバックのクランプ
// 計算)は宣言のみここに置き、実装はEnemyStatusController.cppにある。
// 単純な橋渡し(1行で完結するState向けのgetter/setterや状態遷移トリガー)
// はこれまで通りインラインのままにしている。
// ============================================================
class EnemyStatusController : public ComponentBase, public IMovementSource {
public:
	// patrolDistance: 基準点からどれだけ離れたら折り返すか
	explicit EnemyStatusController(GameObject* owner, float patrolDistance = 3.0f)
		: ComponentBase(owner), patrolDistance_(patrolDistance) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
		postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();

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

		// HP0での死亡通知も同じローカルバス経由で購読する。HealthComponent
		// 側は「died_フラグが立っている間はTakeDamage()を無視する」ため、
		// DiedEventは1回しか発行されない(多重遷移の心配は無い)。
		const SubscriptionId healthSubscriptionId = localBus.Subscribe<HealthComponent::DiedEvent>(
			[this](const HealthComponent::DiedEvent& e) { OnDied(e); });
		healthSubscriber_ = ScopedSubscriber(&localBus, healthSubscriptionId);

		TransitionTo(&stateWalkRight_);
	}

	void Update(float deltaTime) override {
		// 実際の攻撃AI(射程判定、プレイヤーへの接近等)はまだ無いため、
		// 暫定的に一定間隔で武器のHitBoxを短時間有効化するだけの
		// タイマーを回す(詳細はUpdateAttackTimer()参照)。パトロール/
		// ノックバック等のStateMachineとは独立に、常にこのタイマーだけ
		// 先に進める。
		UpdateAttackTimer(deltaTime);

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
		// 死亡中はいかなる状態遷移も受け付けない(IsDead()参照)。
		// OnCollisionEnter()はTakeDamage()の結果として同一フレーム内で
		// 既にStateDeadへ遷移済みの場合があり、そのままここへ来ると
		// StateDead::Enter()直後にStateKnockbackへ上書きされてしまう
		// (StateDead::Update()が一度も呼ばれなくなる不具合の原因だった)。
		if (IsDead()) return;

		stateKnockback_.SetParams(ClampKnockbackParams(params));
		stateMachine_.ForceTransitionTo(this, &stateKnockback_);
	}

	// このEnemyの攻撃がPlayer側にパリィされた時に、Player側から呼ばれる。
	// 小スタン(StateKnockback、被弾時)・大スタン(体幹崩し時、未実装)とは
	// 別の、パリィ専用の短い怯みへ強制遷移する。
	//
	// 現状は敵本体がAttackSourceComponentを直接持つ仮実装のため、
	// GameObject経由でEnemyStatusControllerを直接呼び出す形にしている。
	// 将来Player同様の武器オブジェクトを持つようになったら、武器が
	// 弾かれる物理演出もこの遷移に合わせて追加する想定
	// (詳細はStateParryStun::Enterのコメント参照)。
	void ChangeStateToParryStun() {
		// 死亡中はいかなる状態遷移も受け付けない(IsDead()参照)。
		if (IsDead()) return;

		stateMachine_.ForceTransitionTo(this, &stateParryStun_);
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

	// --- StateDead/StateParryStunから呼ばれる、移動・当たり判定・消滅の橋渡し ---
	// 死亡時(恒久的に停止)とパリィ怯み(一定時間後に再開)の両方で使う
	// 共通の橋渡し。
	//
	// 修正: 以前はprivate:セクションに置かれてしまっており、State側
	// (EnemyState.cppのStateDead/StateParryStun、EnemyStatusControllerの
	// メンバでもfriendでもない別クラス)から呼ぶとコンパイルエラーに
	// なっていた。ApplyKnockbackImpulse等、他のState向け橋渡しメソッドと
	// 同じくpublicへ移動して解決している。
	void SetMovementEnabled(bool enabled) {
		if (movementComponent_ != nullptr) movementComponent_->SetEnabled(enabled);
	}

	void StopMovementForDeath() {
		SetMovementEnabled(false);
	}

	void DisableCollisionForDeath() {
		if (ColliderComponent* collider = GetOwner()->GetComponent<ColliderComponent>()) {
			collider->SetEnabled(false);
		}
	}

	// 一定の猶予(死亡演出の再生時間)を置いた後、StateDead::Update()から
	// 呼ばれる。ObjectManager::Destroy()は実際の削除をFlush()まで遅延する
	// ため、この呼び出し自体はこのフレームの他の処理を壊さない。
	//
	// 自分自身だけでなく、ownedObjects_に登録された所有物(武器・武器
	// ソケット等)もまとめて破棄する。これを行わないと、当たり判定は
	// 無効化済みでも見た目上ワールドに浮いたまま残り続けてしまう
	// (ソケット追従先を失った武器は、直前の姿勢で固まって見える)。
	void RequestDespawn() {
		SceneContext* context = GetOwner()->GetContext();
		if (context == nullptr || context->objectManager == nullptr) return;

		for (Handle<GameObject>& owned : ownedObjects_) {
			if (GameObject* obj = owned.Resolve()) {
				context->objectManager->Destroy(obj);
			}
		}

		context->objectManager->Destroy(GetOwner());
	}

	// このEnemyが生成した(=このEnemyが消えたら道連れで消えるべき)
	// 付随オブジェクトを登録する。EnemyFactory側で武器・武器ソケットの
	// 生成直後に呼んでもらう想定(将来、盾等の装備が増えても同じ仕組みで
	// 対応できる)。
	void RegisterOwnedObject(Handle<GameObject> obj) {
		ownedObjects_.push_back(obj);
	}

	// --- 武器の攻撃判定 --------------------------------------------------
	// EnemyFactory側で、生成した武器のColliderComponent/AttackSourceComponent
	// をここに登録してもらう想定(PlayerStatusController::SetWeapon()と
	// 同じ考え方。武器は別GameObject(ソケット経由でアタッチ)のため、
	// Handle経由の弱参照で保持する)。
	void SetWeapon(Handle<ColliderComponent> weaponCollider, Handle<AttackSourceComponent> weaponAttackSource) {
		weaponCollider_ = weaponCollider;
		weaponAttackSource_ = weaponAttackSource;
	}

private:
	void TransitionTo(IEnemyState* nextState) {
		stateMachine_.TransitionTo(this, nextState);
	}

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr; // ノックバックの実際の移動はこちらに委譲する
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;

	// 武器(別GameObject、ソケット経由でアタッチ)への弱参照。
	// EnemyFactory側からSetWeapon()経由でセットされる想定。
	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;

	// RegisterOwnedObject()で登録された、このEnemyの所有物(武器・武器
	// ソケット等)。死亡時にRequestDespawn()でまとめて破棄する。
	std::vector<Handle<GameObject>> ownedObjects_;

	Math::Vector3 basePosition_{};
	float patrolDistance_;
	Math::Vector3 desiredDirection_{};

	// Stateインスタンス(メモリ断片化を防ぐため実体をメンバで持つ。
	// PlayerStatusControllerと同じ理由)。
	StateWalkRight stateWalkRight_;
	StateWalkLeft stateWalkLeft_;
	StateKnockback stateKnockback_;
	StateDead stateDead_;
	StateParryStun stateParryStun_;

	// 共通StateMachine基盤(PlayerStatusControllerと同じテンプレートを使い回す)
	StateMachine<EnemyStatusController, IEnemyState> stateMachine_;

	// --- 当たり判定連携(実装はEnemyStatusController.cpp) -------------------
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e);

	// HealthComponent::DiedEvent受信時に呼ばれる(実装はEnemyStatusController.cpp)。
	void OnDied(const HealthComponent::DiedEvent& e);

	// --- 簡易的な攻撃タイマー(実装はEnemyStatusController.cpp) --------------
	// 実際の攻撃AI(射程判定、プレイヤーへの接近、攻撃モーション等)が
	// 実装されるまでの暫定処置。一定間隔でSetWeaponHitBoxEnabled(true)し、
	// 一定時間後に閉じるだけ。
	void UpdateAttackTimer(float deltaTime);

	// PlayerStatusController::SetWeaponHitBoxEnabled()と同じ役割
	// (武器のHitBoxのenabled切り替えと、多段ヒット防止用記録のクリア)。
	void SetWeaponHitBoxEnabled(bool enabled);

	// ノックバック中/死亡後/パリィ怯み中は攻撃できない
	// (Stateインスタンスへのポインタ比較で判定。専用の仮想メソッドを
	//  IEnemyStateに増やすほどではないと判断し、最小限の実装にしている)。
	bool CanAttack() const {
		IEnemyState* current = stateMachine_.Current();
		return current != &stateKnockback_ && current != &stateDead_ && current != &stateParryStun_;
	}

	// 死亡中(StateDead)かどうかの共通ガード。CanAttack()と同じくポインタ
	// 比較で判定する。ChangeStateToKnockback()/ChangeStateToParryStun()の
	// ように、外部イベント(被弾/パリィ)起因で呼ばれ得る状態遷移トリガーは
	// 全てこれで死亡中の遷移を弾く。
	//
	// 背景: OnCollisionEnter()内のTakeDamage()がHPを0にすると、同一フレーム
	// 内で同期的にHealthComponent::DiedEvent→OnDied()→StateDead::Enter()まで
	// 実行される。その直後にOnCollisionEnter()が(死亡したことを知らずに)
	// 無条件でChangeStateToKnockback()を呼んでいたため、StateDeadへ遷移した
	// 直後にStateKnockbackへ上書きされ、StateDead::Update()が一度も呼ばれない
	// (かつStateDead::Exit()が空実装のため、無効化した移動/当たり判定も
	//  復元されないままKnockbackに入ってしまう)不具合が起きていた。
	bool IsDead() const {
		return stateMachine_.Current() == &stateDead_;
	}

	// 攻撃の発生間隔(秒)。実際の攻撃AIが実装されたら、この固定間隔ではなく
	// 射程・クールダウン等に基づいた判断へ置き換える想定。
	static constexpr float kAttackInterval = 1.0f;
	// HitBoxが有効になっている時間(秒)。
	static constexpr float kAttackActiveDuration = 0.2f;

	float attackIntervalTimer_ = 0.0f;
	float hitBoxActiveTimer_ = 0.0f;

	ScopedSubscriber subscriber_;
	ScopedSubscriber healthSubscriber_;

	// --- ノックバックの安全域クランプ(地面すり抜け対策、実装は.cpp) ---------
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

	static KnockbackParams ClampKnockbackParams(KnockbackParams params);
	static Math::Vector3 ClampKnockbackDirection(const Math::Vector3& direction);
};