#pragma once
#include <algorithm>
#include <vector>
#include "SkeletonComponent.h"

// ============================================================
// モデルのボーンアニメーション再生"だけ"を責務とするコンポーネント。
//
// 「このモデルをどのパスでいつ描くか」はModelRenderComponentの責務、
// 「今このモデルのポーズがどうあるべきか(どのクリップの何秒目か)」は
// こちらの責務、と完全に分離している(CameraComponentと
// CameraFollowComponentの関係と同じ構図)。
//
// 同じGameObjectに付いているModelRenderComponentが保持する
// KdModelWorkをShared_ptrで共有して直接書き換える。お互いの存在を
// 深く知る必要はなく、共有データ(KdModelWork)を介してのみ繋がる。
//
// アニメーション不要な小道具等はModelRenderComponentだけを付ければ良く、
// 動くキャラクターにだけこちらを追加で付ける、という使い方を想定。
//
// --- クロスフェード(アニメーション間の滑らかな遷移)について -----------
// KdAnimator::AdvanceTime()はボーンのローカル行列を毎フレーム直接
// 上書きするだけで、それ自体はブレンド機構を持たない。Play()で
// 別アニメーションに切り替えると、切り替わった瞬間に前のポーズから
// 新アニメーションの先頭ポーズへ不連続にジャンプしてしまう。
//
// これを緩和するため、Play()で切り替わる直前の全ボーンのローカル行列
// (blendFromLocalTransforms_)をスナップショットしておき、切り替え後
// blendDuration_秒間だけ「スナップショット→新アニメーションの今の
// ポーズ」を毎フレーム補間してWorkNodes()を上書きする。
// 行列をそのままLerpすると回転部分が歪む(せん断が入る)ため、
// 位置・回転・拡縮に分解してから個別に補間する(BlendMatrix参照)。
// 回転の補間はSlerpを使うが、2つのクォータニオンの内積が負の場合
// 最短経路にならず不自然な回り込みが起きるため、符号を補正してから
// Slerpする(BlendMatrix内のコメント参照)。
//
// 前のアニメーションが動いている最中に別アニメーションへ割り込んだ
// 場合でも、ブレンド元は「割り込まれた瞬間の静止ポーズ」であり、
// 前のアニメーションの動きの勢いまでは引き継がない(厳密な二重評価
// ブレンドではなく簡易的なfrozen-poseクロスフェード)。今の技構成
// (Windup/Active/Recovery/Stagger等、切り替わり自体が意味のある
// 区切りになっている)であれば実用上問題にならない想定。
//
// KdAnimation.cpp(KdAnimator/KdAnimationData)側は一切変更していない。
// ============================================================
class ModelAnimatorComponent : public ComponentBase
{
public:
	explicit ModelAnimatorComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		// ファクトリー内でPlayするため
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
	}

	void Start() override
	{
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
	}

	// アニメーション名を指定して再生開始
	// ・animName				… KdModelData内のアニメーション名(glTFのアニメーションクリップ名)
	// ・loop					… ループ再生するか
	// ・targetDurationSeconds	… 指定した場合、クリップの実際の長さ(m_maxLength)に
	//   関わらず、ちょうどこの秒数で再生し終わるよう再生速度を自動スケーリングする。
	//   省略(-1以下)した場合はSetFPS()で設定した速度のまま再生する。
	//
	//   攻撃/回避のように、ゲームプレイ側の秒数(AttackMoveData::windupDuration
	//   等)を戦闘バランスの基準にしたい行動に使う想定。アニメーション側の
	//   尺を毎回手で合わせ込む代わりに、ゲームプレイ側の数値を基準として
	//   アニメーション側を追従させる(詳細はチャット上の設計方針の議論を参照)。
	//   再生速度を大きく伸縮するとモーションの「重さ」が損なわれるため、
	//   本番アセットで極端な倍率が必要になる技は個別に手動調整すること。
	void Play(std::string_view animName, bool loop = true, float targetDurationSeconds = -1.0f)
	{
		if (!skeleton_) { return; }

		auto animData = skeleton_->WorkModel().GetAnimation(animName);
		if (!animData) {
			assert(0 && "ModelAnimatorComponent ファイルが存在しません。ファイルパスを確認してください");
			return;
		}

		// 既に同じアニメーションを再生中なら、頭出ししない
		// (毎フレームPlay()を呼ぶような使い方をしても、再生位置が
		//  リセットされ続けてしまわないようにするための簡易ガード)
		if (spNowPlaying_ == animData) { return; }

		// 遷移前のポーズをスナップショットしておく(クロスフェード用)。
		// 初回再生(まだ何も再生していない状態からの開始)はブレンド元の
		// ポーズが存在しないため、ブレンドせず即座に切り替える。
		if (spNowPlaying_ != nullptr) {
			const auto& nodes = skeleton_->WorkModel().WorkNodes();
			blendFromLocalTransforms_.resize(nodes.size());
			for (size_t i = 0; i < nodes.size(); ++i) {
				blendFromLocalTransforms_[i] = nodes[i].m_localTransform;
			}
			blendElapsed_ = 0.0f;
		}
		else {
			blendElapsed_ = blendDuration_; // ブレンド不要
		}

		spNowPlaying_ = animData;
		animator_.SetAnimation(animData, loop);

		// targetDurationSecondsが指定された場合、m_maxLength(このクリップの
		// 実際の長さ)と目標秒数の比から「1秒あたりに進めるアニメーション
		// 時間」を逆算し、以降のUpdate()ではSetFPS()の値ではなくこちらを使う。
		if (targetDurationSeconds > 0.0f && animData->m_maxLength > 0.0f) {
			targetSpeedOverride_ = animData->m_maxLength / targetDurationSeconds;
		}
		else {
			targetSpeedOverride_ = -1.0f; // 通常のfps基準に戻す
		}
	}

	// 再生速度をfps換算で指定して毎フレーム進行させる
	// ・fps		… 1秒間に進めるアニメーションのフレーム数
	//   ※ KdGLTFLoader側でアニメーションキーの時間を「秒」で読み込んでいるか
	//     「フレーム数」で読み込んでいるかによって、ここで渡すべき値の意味が
	//     変わる。もし秒単位ならfps引数には単純にdeltaTimeを渡すこと。
	void Update(float deltaTime) override
	{
		if (!skeleton_) { return; }

		// targetSpeedOverride_が設定されていれば(Play()にtargetDurationSeconds
		// を渡した場合)、SetFPS()の値ではなくこちらを優先して速度を決める。
		const float speed = (targetSpeedOverride_ > 0.0f) ? targetSpeedOverride_ : m_fps;
		animator_.AdvanceTime(skeleton_->WorkModel().WorkNodes(), deltaTime * speed);

		// クロスフェード: 遷移直後のblendDuration_秒間、直前のポーズと
		// 新しいアニメーションの今のポーズを補間してWorkNodes()を上書きする。
		if (blendElapsed_ < blendDuration_) {
			blendElapsed_ += deltaTime;
			const float t = std::min(1.0f, blendElapsed_ / blendDuration_);

			auto& nodes = skeleton_->WorkModel().WorkNodes();
			const size_t count = std::min(nodes.size(), blendFromLocalTransforms_.size());
			for (size_t i = 0; i < count; ++i) {
				nodes[i].m_localTransform = BlendMatrix(blendFromLocalTransforms_[i], nodes[i].m_localTransform, t);
			}
		}
	}

	// 再生速度の基準fpsを設定(デフォルト60fps)
	void SetFPS(float fps) { m_fps = fps; }

	// クロスフェードの時間(秒)を設定する。技の種類によって
	// 「素早く切り替えたい/じっくり繋ぎたい」が変わる場合はここを調整する。
	void SetBlendDuration(float seconds) { blendDuration_ = seconds; }

	// 現在のアニメーションが最後まで再生し終わったか(ループ再生時は常にfalse)
	bool IsAnimationEnd() const { return animator_.IsAnimationEnd(); }

private:
	// 2つのローカル変換行列を、位置・回転・拡縮に分解してから個別に補間する。
	// 行列のままLerpすると回転部分が歪む(せん断が入る)ため、
	// 位置・拡縮はLerp、回転はSlerpで別々に補間してから合成し直す。
	static Math::Matrix BlendMatrix(const Math::Matrix& from, const Math::Matrix& to, float t)
	{
		// Matrix::Decompose()はconstメンバ関数ではないため、
		// const参照のfrom/toに対して直接は呼び出せない。
		// ローカルの非constコピーを作ってから呼び出す。
		Math::Matrix fromCopy = from;
		Math::Matrix toCopy = to;

		Math::Vector3 fromScale, fromTrans;
		Math::Quaternion fromRot;
		fromCopy.Decompose(fromScale, fromRot, fromTrans);

		Math::Vector3 toScale, toTrans;
		Math::Quaternion toRot;
		toCopy.Decompose(toScale, toRot, toTrans);

		// Slerpは2つのクォータニオンの内積が負だと最短経路を通らず、
		// ブレンド中に不自然な回り込みが起きる。片方の符号を反転させて
		// 内積を正にしてからSlerpする(クォータニオンq と -q は同じ回転を
		// 表すため、符号反転しても結果の姿勢は変わらない)。
		if (fromRot.Dot(toRot) < 0.0f) {
			toRot = Math::Quaternion(-toRot.x, -toRot.y, -toRot.z, -toRot.w);
		}

		const Math::Vector3 blendedScale = Math::Vector3::Lerp(fromScale, toScale, t);
		const Math::Quaternion blendedRot = Math::Quaternion::Slerp(fromRot, toRot, t);
		const Math::Vector3 blendedTrans = Math::Vector3::Lerp(fromTrans, toTrans, t);

		return Math::Matrix::CreateScale(blendedScale)
			* Math::Matrix::CreateFromQuaternion(blendedRot)
			* Math::Matrix::CreateTranslation(blendedTrans);
	}

	SkeletonComponent* skeleton_ = nullptr;

	KdAnimator							animator_;

	// 現在再生中のアニメーションデータ(Play()の多重頭出し防止用)
	std::shared_ptr<KdAnimationData>	spNowPlaying_ = nullptr;

	float								m_fps = 60.0f;

	// Play()にtargetDurationSecondsが渡された場合、m_fpsの代わりに使う
	// 「1秒あたりに進めるアニメーション時間」。-1以下ならm_fps基準に戻す。
	float								targetSpeedOverride_ = -1.0f;

	// --- クロスフェード用のワーク ---------------------------------------
	std::vector<Math::Matrix>			blendFromLocalTransforms_; // 切り替わる直前の全ボーンのローカル行列
	float								blendDuration_ = 0.15f;    // ブレンドにかける時間(秒)
	float								blendElapsed_ = 0.0f;      // ブレンド開始からの経過時間(blendDuration_以上ならブレンド終了)
};