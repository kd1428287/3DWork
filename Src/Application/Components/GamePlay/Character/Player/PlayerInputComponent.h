#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"
#include "../../../Tags/IMovementSource.h"

#include "../../Camera/CameraComponent.h"
#include "../../Camera/CameraOrbitComponent.h"

// 先行入力バッファの1エントリ。Attack/Evadeのようなタップ入力のみを扱う。
// パリィはGuard開始直後の時間窓として表現するため、ここには含まれない。
struct BufferedInput
{
	ActionCommand command;
	float timeRemaining; // 先行入力が有効な残り時間（秒、例: 0.2秒）

	// このコマンドが積まれた瞬間の移動入力方向(正規化済み)のスナップショット。
	// 方向を使わないコマンド(Attack等)では単に無視される。
	// バッファ滞留中に入力方向が変わっても、積まれた瞬間の意図がブレないようにする。
	Math::Vector3 direction = Math::Vector3::Zero;
};

// プレイヤーの入力状態を保持するコンポーネント。
// KdInputManagerから得た生入力を自ら読み取り、移動方向についてはカメラ相対の
// 変換まで内部で完結させる(旧InputSystemが担っていた変換ロジックを移管)。
class PlayerInputComponent : public ComponentBase, public IMovementSource {
public:
	explicit PlayerInputComponent(GameObject* owner) : ComponentBase(owner) {}

	// カメラのactiveCamera(SceneContext経由)が使えない/yawを決められない場合に
	// フォールバック先として参照するCameraOrbitComponent。
	// 別GameObject(カメラリグ)上のコンポーネントのためHandle<T>で保持する。
	void SetCameraOrbitFallback(Handle<CameraOrbitComponent> orbit) { cameraOrbitFallback_ = orbit; }

	// GameObjectのUpdate、またはPreUpdateで毎フレーム呼び出す。
	// 先行入力バッファの有効期限を減算しつつ、KdInputManagerから今フレームの
	// 生入力を読み取ってmoveDirection_/dashHeld_等へ反映する。
	void PreUpdate(float deltaTime) override {
		// --- 先行入力バッファの寿命管理 -------------------------------
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end();) {
			it->timeRemaining -= deltaTime;
			if (it->timeRemaining <= 0.0f) {
				it = inputBuffer_.erase(it); // 有効期限切れは削除
			}
			else {
				++it;
			}
		}

		// --- 移動方向の取得とカメラ相対変換 ---------------------------
		const Math::Vector2 axis = KdInputManager::Instance().GetAxisState("Move");
		Math::Vector3 dir{ axis.x, 0.0f, axis.y };

		// カメラの水平方向(yaw)を移動方向の基準にする
		bool usedActualCameraForward = false;
		if (SceneContext* context = GetOwner()->GetContext()) {
			if (CameraComponent* camera = context->activeCamera) {
				Math::Vector3 camForward = camera->GetForward();
				camForward.y = 0.0f;
				if (camForward.LengthSquared() > kMinCameraForwardLengthSq) {
					camForward.Normalize();
					const float yaw = std::atan2(-camForward.x, -camForward.z);
					const Math::Quaternion yawOnly =
						Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, yaw);
					dir = Math::Vector3::Transform(dir, yawOnly);
					usedActualCameraForward = true;
				}
			}
		}

		// activeCameraが無い、あるいはカメラがほぼ真上/真下を向いていて
		// yawを決められない場合は、CameraOrbitComponentの軌道角度に
		// フォールバックする(未設定(固定カメラ等)の場合はワールド軸に
		// 対する入力としてそのまま扱う)。
		if (!usedActualCameraForward) {
			if (CameraOrbitComponent* orbit = cameraOrbitFallback_.Resolve()) {
				const Math::Quaternion yawOnly =
					Math::Quaternion::CreateFromYawPitchRoll(orbit->GetYaw(), 0.0f, 0.0f);
				dir = Math::Vector3::Transform(dir, yawOnly);
			}
		}

		SetMoveDirection(dir);

		// --- 継続入力(押している間ずっと反映) -----------------------
		dashHeld_ = KdInputManager::Instance().IsHold("Dash");
		guardHeld_ = KdInputManager::Instance().IsHold("Guard");

		// --- 単発入力(押した瞬間だけバッファへ積む/フラグを立てる) -----
		if (KdInputManager::Instance().IsPress("Attack")) {
			PushCommand(ActionCommand::Attack);
		}
		if (KdInputManager::Instance().IsPress("Evade")) {
			PushCommand(ActionCommand::Evade);
		}
		if (KdInputManager::Instance().IsPress("Lock")) {
			lockPressed_ = true;
		}
	}

	// --- 外部（PlayerStatusControllerなど）が先行入力を確認/消費する関数 ---

	// 覗き見用。実行可能かどうかを先に判定してからConsumeCommand()を
	// 呼びたい場合に使う。これを経由せずいきなりConsumeCommand()を呼ぶと、
	// まだ猶予が残っている入力を実行不可なタイミングで誤って消費してしまう。
	bool HasCommand(ActionCommand command) const {
		for (const auto& input : inputBuffer_) {
			if (input.command == command) return true;
		}
		return false;
	}

	bool ConsumeCommand(ActionCommand command) {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end(); ++it) {
			if (it->command == command) {
				inputBuffer_.erase(it); // 消費したためバッファから消す
				return true;
			}
		}
		return false;
	}

	// 方向スナップショットも合わせて取り出したい場合(Evade等)はこちら。
	bool ConsumeCommand(ActionCommand command, Math::Vector3& outDirection) {
		for (auto it = inputBuffer_.begin(); it != inputBuffer_.end(); ++it) {
			if (it->command == command) {
				outDirection = it->direction;
				inputBuffer_.erase(it);
				return true;
			}
		}
		return false;
	}

	bool IsGuardHeld() const { return guardHeld_; }
	bool IsDashHeld() const { return dashHeld_; }

	// このフレームLockが押されていたかを取り出し、同時にフラグを消費(false)する。
	// HasCommand()に相当する「覗き見だけ」の版は用意していない。Lockは
	// CanStart*系のような実行可否判定を挟まず常に処理してよい入力のため、
	// ConsumeCommand()相当の一発だけで足りる。
	bool ConsumeLockPressed() {
		bool pressed = lockPressed_;
		lockPressed_ = false;
		return pressed;
	}

	// 移動方向とDashキーの状態から、今フレームの移動意思をMovementStateとして
	// 解決する。キーボード操作前提のため、アナログの傾き量ではなく
	// Dashキーが押されているか否かでWalk/Runを切り替える。
	MovementState GetDesiredMovementState() const {
		if (moveDirection_.LengthSquared() <= 0.0f) return MovementState::Stand;
		return dashHeld_ ? MovementState::Run : MovementState::Walk;
	}

	// IMovementSourceの実装
	Math::Vector3 GetDesiredVelocity() override {
		return moveDirection_;
	}

	// moveDirection_の読み取り専用アクセサ。GetDesiredVelocity()は
	// IMovementSource側の都合でconstにできないため、const参照からも
	// 呼べる版を別途用意する(PlayerStatusController::HandleMovementInput
	// がWalk中の前後左右判定に使う。ClassifyEvadeDirection参照)。
	Math::Vector3 GetMoveDirection() const { return moveDirection_; }

private:
	void SetMoveDirection(Math::Vector3 direction) {
		direction.Normalize();
		moveDirection_ = direction;
	}

	// ボタンが押された瞬間に呼ばれる（バッファにキューイング）
	// この瞬間のmoveDirection_を正規化してスナップショットしておく
	// (Evadeの回避方向など、方向を伴うコマンド向け)。
	void PushCommand(ActionCommand command, float bufferTime = 0.2f) {
		Math::Vector3 dir = moveDirection_;
		if (dir.LengthSquared() > kDirectionEpsilon) {
			dir.Normalize();
		}
		else {
			dir = Math::Vector3::Zero;
		}
		inputBuffer_.push_back({ command, bufferTime, dir });
	}

	Math::Vector3 moveDirection_;
	bool dashHeld_ = false;
	bool guardHeld_ = false;
	bool lockPressed_ = false; // このフレームLockが押されたか(消費されるまで保持)
	std::vector<BufferedInput> inputBuffer_; // 先行入力バッファ(Attack/Evadeのみ)

	Handle<CameraOrbitComponent> cameraOrbitFallback_; // activeCamera不使用時のyawフォールバック先

	// activeCameraの水平前方ベクトルがこれ以下(ほぼ真上/真下を向いている)
	// の場合は、そこからyawを決めずCameraOrbitComponent側にフォールバックする、
	// という閾値。
	static constexpr float kMinCameraForwardLengthSq = 1e-6f;

	static constexpr float kDirectionEpsilon = 1e-6f; // PushCommand()の方向正規化用しきい値
};