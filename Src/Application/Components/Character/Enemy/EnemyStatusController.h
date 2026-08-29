#pragma once
#include <string>
#include <vector>
#include "EnemyState.h"
#include "EnemyStatusData.h"
#include "../../Movement/IMovementSource.h"
#include "../../Movement/MovementComponent.h"
#include "../../Movement/VelocityComponent.h"
#include "../../Transform/TransformComponent.h"
#include "../StateMachine/StateMachine.h"
#include "../../Animation/ModelAnimatorComponent.h"
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
//
// --- 派生クラスについて ---------------------------------------------
// このクラスは雑魚敵(BruteStatusController)・ボス(BossStatusController)の
// 共通基底として使う。パトロール/ノックバック/死亡/パリィ怯みといった
// 共通の挙動はここに残し、敵種ごとに差し替えたい部分(攻撃AI)だけを
// virtualにして派生クラス側で上書きできるようにしている。
//
// 【変更履歴: 攻撃AIをタイマー駆動からBT駆動へ】
// 以前はUpdateAttackTimer()という一定間隔で自動的に武器のHitBoxを
// 有効化するだけの暫定タイマーだったが、EnemyBTController(ビヘイビア
// ツリー)がプレイヤーとの距離を見て開始タイミングを判断できるように
// なったため撤去した。代わりに、BT側のAction(EnemyActionAttack)から
// 呼ばれるTryStartAttack()(virtual)を新しい拡張ポイントにしている。
// Boss等が複数パターンの攻撃AIに差し替えたい場合は、UpdateAttackTimer()
// ではなくTryStartAttack()をoverrideすること。
//
// State(StateWalkRight等)はEnemyStatusController*を扱うだけなので、
// 派生クラスをそのままStateMachineに乗せても変更不要。
//
// チューニング値(patrolDistance、攻撃タイミング等)はEnemyStatusData
// (別ファイル)にまとめてコンストラクタで受け取る形にした。将来的に
// JSON等の外部データから読み込んだEnemyStatusDataをそのまま渡せる
// ようにするための下準備(実際のJSON読み込み処理自体はまだ未実装)。
// ============================================================
class EnemyStatusController : public ComponentBase, public IMovementSource {
public:
	explicit EnemyStatusController(GameObject* owner, const EnemyStatusData& data = EnemyStatusData{})
		: ComponentBase(owner), data_(data) {}

	void Start() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		movementComponent_ = GetOwner()->GetComponent<MovementComponent>();
		velocityComponent_ = GetOwner()->GetComponent<VelocityComponent>();
		postureComponent_ = GetOwner()->GetComponent<PostureComponent>();
		healthComponent_ = GetOwner()->GetComponent<HealthComponent>();
		modelAnimatorComponent_ = GetOwner()->GetComponent<ModelAnimatorComponent>();

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
		// 攻撃AIはBT(EnemyBTController)がTryStartAttack()を呼ぶことで
		// 開始される。ここでは(以前のUpdateAttackTimer()のような)独自の
		// タイマーは回さず、StateMachineの更新に徹する
		// (EnemyStateAttack自身のWindup/Active/Recovery進行はstateMachine_.Update()
		//  経由でEnemyStateAttack::Update()が担当する)。
		stateMachine_.Update(this, deltaTime);
	}

	// --- Stateから参照される情報 ------------------------------------------

	const Math::Vector3& GetBasePosition() const { return basePosition_; }
	float GetPatrolDistance() const { return data_.patrolDistance; }

	Math::Vector3 GetCurrentPosition() const {
		return transform_ ? transform_->GetPosition() : basePosition_;
	}

	// EnemyStateAttackが参照する攻撃1回分の時間パラメータ。
	float GetAttackWindupDuration() const { return data_.attackWindupDuration; }
	float GetAttackActiveDuration() const { return data_.attackActiveDuration; }
	float GetAttackRecoveryDuration() const { return data_.attackRecoveryDuration; }

	// EnemyBTController::IsPlayerInAttackRange()が参照する間合い。
	float GetAttackRange() const { return data_.attackRange; }

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

	// --- 攻撃AI(BT駆動) --------------------------------------------------
	// EnemyBTController::EnemyActionAttackから、プレイヤーが射程内に
	// いる間毎フレーム呼ばれる想定。virtualにしてあるので、Boss等で
	// 複数パターンから技を選ぶ攻撃AIに差し替えたい場合はoverrideすること
	// (以前のUpdateAttackTimer()に代わる拡張ポイント)。
	//
	// targetPositionは攻撃開始時に振り向かせたい座標(通常はプレイヤーの
	// 現在位置)。BT側は射程判定のためにどのみちプレイヤー座標を持って
	// いるので、ついでにここへ渡してもらう形にした。パトロール
	// (StateWalkRight/Left)はX軸移動のみの簡易実装で、攻撃中は
	// SetDesiredDirection(Zero)で移動を止めるため回転の手がかりが無く、
	// このターゲット指定が無いと直前のパトロール方向を向いたまま
	// 攻撃してしまう(実際に見た目上そうなっていた挙動)。
	//
	// 既に攻撃中(IsAttacking())なら何もせずtrueを返す(idempotent)。
	// これにより、BT側が「攻撃中かどうかを気にせず毎フレーム呼ぶ」だけで
	// 安全に使える(呼ぶたびにWindupから再始動してしまう事故を防ぐ)。
	// 向き直しも開始の一瞬だけ(Windup開始時に1回スナップ)で、Player側の
	// FaceAttackTarget()と同じく攻撃中に追従はしない。
	virtual bool TryStartAttack(const Math::Vector3& targetPosition) {
		if (IsAttacking()) return true;
		if (!CanAttack()) return false;

		FaceHorizontalTarget(targetPosition);
		stateMachine_.ForceTransitionTo(this, &stateAttack_);
		return true;
	}

	bool IsAttacking() const { return stateMachine_.Current() == &stateAttack_; }

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

	// 武器のHitBoxのenabled切り替えと、多段ヒット防止用記録のクリア
	// (PlayerStatusController::SetWeaponHitBoxEnabled()と同じ役割)。
	// 以前はUpdateAttackTimer()専用の内部処理としてprotectedだったが、
	// EnemyStateAttack(EnemyState.h/.cpp、別クラス階層)からも呼ぶ必要があるため
	// publicへ変更した(PlayerStatusController::SetWeaponHitBoxEnabled()も
	// 同様にpublic)。
	void SetWeaponHitBoxEnabled(bool enabled);

	// --- アニメーション再生 -----------------------------------------------
	// PlayerStatusController::PlayAnimation()と同じ考え方。State側が
	// 具体的なModelAnimatorComponentを直接知らずに再生できるようにする
	// 薄いラッパー。
	void PlayAnimation(const std::string& name, bool loop = false, float targetDurationSeconds = -1.0f) {
		if (modelAnimatorComponent_ != nullptr) {
			modelAnimatorComponent_->Play(name, loop, targetDurationSeconds);
		}
	}

protected:
	// --- 派生クラス(Brute/Boss)が差し替える・再利用するための拡張ポイント ---

	// ノックバック中/死亡後/パリィ怯み中/攻撃中は新たな攻撃を開始できない
	// (Stateインスタンスへのポインタ比較で判定)。TryStartAttack()内部で
	// 使う。「攻撃中は攻撃できない」は一見当たり前だが、TryStartAttack()
	// 側で先にIsAttacking()をチェックして継続扱いにしているため、実際に
	// ここへ来るのは「まだ攻撃していない」ケースだけになる。
	bool CanAttack() const {
		IEnemyState* current = stateMachine_.Current();
		return current != &stateKnockback_ && current != &stateDead_ && current != &stateParryStun_;
	}

	// 死亡中(StateDead)かどうかの共通ガード。
	bool IsDead() const {
		return stateMachine_.Current() == &stateDead_;
	}

	// コンストラクタで受け取ったチューニング値。派生クラスの
	// TryStartAttack()等から直接参照できるようprotectedにしている。
	EnemyStatusData data_;

private:
	void TransitionTo(IEnemyState* nextState) {
		stateMachine_.TransitionTo(this, nextState);
	}

	TransformComponent* transform_ = nullptr;
	MovementComponent* movementComponent_ = nullptr;
	VelocityComponent* velocityComponent_ = nullptr; // ノックバックの実際の移動はこちらに委譲する
	PostureComponent* postureComponent_ = nullptr;
	HealthComponent* healthComponent_ = nullptr;
	ModelAnimatorComponent* modelAnimatorComponent_ = nullptr;

	// 武器(別GameObject、ソケット経由でアタッチ)への弱参照。
	// EnemyFactory側からSetWeapon()経由でセットされる想定。
	Handle<ColliderComponent> weaponCollider_;
	Handle<AttackSourceComponent> weaponAttackSource_;

	// RegisterOwnedObject()で登録された、このEnemyの所有物(武器・武器
	// ソケット等)。死亡時にRequestDespawn()でまとめて破棄する。
	std::vector<Handle<GameObject>> ownedObjects_;

	Math::Vector3 basePosition_{};
	Math::Vector3 desiredDirection_{};

	// Stateインスタンス(メモリ断片化を防ぐため実体をメンバで持つ。
	// PlayerStatusControllerと同じ理由)。
	StateWalkRight stateWalkRight_;
	StateWalkLeft stateWalkLeft_;
	StateKnockback stateKnockback_;
	StateDead stateDead_;
	StateParryStun stateParryStun_;
	EnemyStateAttack stateAttack_; // PlayerState.h側のStateAttackとの名前衝突回避(EnemyState.h参照)

	// 共通StateMachine基盤(PlayerStatusControllerと同じテンプレートを使い回す)
	StateMachine<EnemyStatusController, IEnemyState> stateMachine_;

	// --- 当たり判定連携(実装はEnemyStatusController.cpp) -------------------
	void OnCollisionEnter(const CollisionSystem::CollisionEnterEvent& e);

	// HealthComponent::DiedEvent受信時に呼ばれる(実装はEnemyStatusController.cpp)。
	void OnDied(const HealthComponent::DiedEvent& e);

	ScopedSubscriber subscriber_;
	ScopedSubscriber healthSubscriber_;

	// --- ノックバックの安全域クランプ(地面すり抜け対策、実装は.cpp) ---------
	//
	// 恒久対応(CCD)ではなく、あくまで軽量な緩和策である点に注意。
	// 「1フレームでの最大移動量」を大きく超えるような異常な速度を防ぐのが
	// 目的で、地形の厚みそのものが薄すぎる場合は根本対策にならない
	// (別途、地面コライダーに十分な厚みを持たせることも推奨)。
	//
	// data_(EnemyStatusData)のmaxKnockbackSpeed/minKnockbackDirectionYを
	// 参照するようになったため、static関数からインスタンスメソッドに変更した。
	KnockbackParams ClampKnockbackParams(KnockbackParams params);
	Math::Vector3 ClampKnockbackDirection(const Math::Vector3& direction);

	// --- 攻撃開始時の向き直し(実装は.cpp) -----------------------------
	// TryStartAttack()から呼ばれる。targetPositionへ水平方向だけ
	// 一瞬で振り向かせる(Slerpによる補間はしない、Player側の
	// FaceAttackTarget()と同じ簡易実装)。
	//
	// 【要確認】TransformComponentの回転設定APIの実際のシグネチャに
	// 合わせて調整すること(atan2+Quaternion::CreateFromAxisAngleを
	// 仮定して書いている。PlayerStatusController::FaceAttackTarget()の
	// 同種コメント参照)。
	void FaceHorizontalTarget(const Math::Vector3& targetPosition);
};