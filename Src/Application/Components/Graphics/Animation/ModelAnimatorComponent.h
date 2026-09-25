#pragma once
#include "SkeletonComponent.h"
#include "RootMotionExtractor.h"
#include "Application/Definitions/Character/Common/CharacterDefinitionCommon.h"

// 初期設定(Prefab/JSONから渡す値)。rootMotionはRootMotionExtractor.hのRootMotionConfigをそのまま使う。
struct ModelAnimatorConfig
{
	float fps = 60.0f;
	float speedScale = 1.0f;
	float blendDuration = 0.15f;
	RootMotionConfig rootMotion;
};

// モデルのボーンアニメーション再生
class ModelAnimatorComponent : public ComponentBase
{
public:
	using Config = ModelAnimatorConfig;

	explicit ModelAnimatorComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetConfig(const Config& config)
	{
		SetFPS(config.fps);
		SetSpeedScale(config.speedScale);
		SetBlendDuration(config.blendDuration);
		SetRootMotionConfig(config.rootMotion);
	}

	void Awake() override
	{
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
	}

	void Play(const MotionClipData& clip)
	{
		SetRootMotionEnabled(clip.useRootMotion);
		SetBlendDuration(clip.blendDuration);
		//Play(clip.animationName, clip.loop, clip.duration, clip.startFrame, clip.endFrame, (clip.startFrame != 0 && clip.endFrame != 0));
		Play(clip.animationName, clip.loop, clip.duration, clip.startFrame, clip.endFrame, true);
	}

	// アニメーション名を指定して再生開始
	// ・animName				… アニメーション名
	// ・loop					… ループ再生するか
	// ・targetDurationSeconds	… 指定した場合、クリップの実際の長さ(m_maxLength)に
	//   関わらず、ちょうどこの秒数で再生し終わるよう再生速度を自動スケーリングする
	// ・startFrame/endFrame	… フレーム番号をそのまま指定する。
	//   endFrameを0以下にすると、クリップの末尾まで再生する
	void Play(std::string_view animName, bool loop = true, float targetDurationSeconds = -1.0f, int startFrame = 0, int endFrame = 0, bool force = false)
	{
		if (!skeleton_ || animName.empty()) { return; }

		auto animData = skeleton_->WorkModel().GetAnimation(animName);
		if (!animData) {
			assert(0 && "ModelAnimatorComponent ファイルが存在しません。ファイルパスを確認してください");
			return;
		}

		// 既に同じアニメーションを再生中なら再生しない
		if (!force && spNowPlaying_ == animData) { return; }

		// FootstepEventComponent等がクリップ切り替えを検知するために保持しておく
		currentAnimName_ = animName;

		// 遷移前のポーズをスナップショットしておく
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

		// 同じクリップの続き(フェーズ切替)かどうか。spNowPlaying_を更新する前に判定する
		const bool continuesClip = (spNowPlaying_ == animData);

		spNowPlaying_ = animData;
		animator_.SetAnimation(animData, loop, startFrame, endFrame);

		// 切替直後の1フレームが移動量にならないよう基準を取り直させる。同一クリップなら固定位置は維持する
		rootMotion_.NotifyAnimationChanged(continuesClip);

		const float duration = animator_.GetDuration();
		if (targetDurationSeconds > 0.0f && duration > 0.0f) {
			targetSpeedOverride_ = duration / targetDurationSeconds;
		}
		else {
			targetSpeedOverride_ = -1.0f; // 通常のfps基準に戻す
		}
	}

	// 再生速度をfps換算で指定して毎フレーム進行させる
	// ・fps		… 1秒間に進めるアニメーションのフレーム数
	void AdvanceFK(float deltaTime)
	{
		if (!skeleton_) { return; }

		rootMotion_.PrepareFrame(skeleton_->WorkModel());

		// targetSpeedOverride_が設定されていれば優先して速度を決める。
		const float speed = (targetSpeedOverride_ > 0.0f) ? targetSpeedOverride_ : speedScale_;
		const float timeBeforeAdvance = animator_.GetTime();
		animator_.AdvanceTime(skeleton_->WorkModel().WorkNodes(), deltaTime * speed);

		// 時間が戻った=ループで先頭(またはstartFrame)へ巻き戻ったフレーム
		const bool wrapped = animator_.GetTime() < timeBeforeAdvance;
		rootMotion_.FinalizeFrame(wrapped);

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
	void SetFPS(float fps) { animator_.SetFPS(fps); }
	void SetSpeedScale(float scale) { speedScale_ = scale; }

	// クロスフェードの時間(秒)を設定する。技の種類によって
	// 「素早く切り替えたい/じっくり繋ぎたい」が変わる場合はここを調整する。
	void SetBlendDuration(float seconds) { blendDuration_ = seconds; }

	// アニメーション専用glTFを読み込み、このモデルで再生できるようにする(モデル設定後に呼ぶ)
	bool LoadAnimationFile(std::string_view filePath)
	{
		if (!skeleton_) { return false; }

		auto spData = skeleton_->WorkModel().GetData();
		return spData && spData->LoadAnimationFile(filePath);
	}

	// フォルダ内のアニメーションglTFを一括で読み込む(戻り値は読み込めたファイル数)
	int LoadAnimationDirectory(std::string_view dirPath)
	{
		if (!skeleton_) { return 0; }

		auto spData = skeleton_->WorkModel().GetData();
		return spData ? spData->LoadAnimationDirectory(dirPath) : 0;
	}

	// 現在のアニメーションが最後まで再生し終わったか(ループ再生時は常にfalse)
	bool IsAnimationEnd() const { return animator_.IsAnimationEnd(); }

	// 現在再生中のアニメーション名(FootstepEventComponent等のクリップ切り替え検知用)
	std::string_view GetCurrentAnimationName() const { return currentAnimName_; }

	// 現在の再生位置を0.0〜1.0の比率で返す。速度スケーリングの影響を受けないため、
	// 接地タイミング等を実時間ではなく比率で判定したい場合に使う。
	float GetNormalizedTime() const
	{
		return animator_.GetNormalizedTime();
	}

	// --- ルートモーション ---------------------------------------------
	// 抽出設定を丸ごと渡す。boneNameが空なら無効。
	void SetRootMotionConfig(const RootMotionConfig& config) { rootMotion_.SetConfig(config); }
	const RootMotionConfig& GetRootMotionConfig() const { return rootMotion_.GetConfig(); }

	// 実行時の切替。ボーン名などはSetRootMotionConfigで設定した値を使う。
	void SetRootMotionEnabled(bool enabled) { rootMotion_.SetActive(enabled); }
	void SetRootMotionExtractRotation(bool enabled) { rootMotion_.SetExtractRotation(enabled); }

	// 蓄積された移動量/Yaw量を取り出す(呼ぶと0に戻る)
	Math::Vector3 ConsumeRootMotionDelta() { return rootMotion_.ConsumeDelta(); }
	float ConsumeRootMotionYawDelta() { return rootMotion_.ConsumeYawDelta(); }

private:
	// 2つのローカル変換行列を分解してから個別に補間する
	static Math::Matrix BlendMatrix(const Math::Matrix& from, const Math::Matrix& to, float t)
	{
		Math::Matrix fromCopy = from;
		Math::Matrix toCopy = to;

		Math::Vector3 fromScale, fromTrans;
		Math::Quaternion fromRot;
		fromCopy.Decompose(fromScale, fromRot, fromTrans);

		Math::Vector3 toScale, toTrans;
		Math::Quaternion toRot;
		toCopy.Decompose(toScale, toRot, toTrans);

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

	// 現在再生中のアニメーションデータ
	std::shared_ptr<KdAnimationData>	spNowPlaying_ = nullptr;

	// 現在再生中のアニメーション名(GetCurrentAnimationName()用)
	std::string							currentAnimName_;

	float								speedScale_ = 1.0f;

	// Play()にtargetDurationSecondsが渡された場合、m_fpsの代わりに使う
	float								targetSpeedOverride_ = -1.0f;

	std::vector<Math::Matrix>			blendFromLocalTransforms_; // 切り替わる直前の全ボーンのローカル行列
	float								blendDuration_ = 0.15f;    // ブレンドにかける時間(秒)
	float								blendElapsed_ = 0.0f;      // ブレンド開始からの経過時間(blendDuration_以上ならブレンド終了)

	// ボーン解決・巻き戻り時の扱い・基準位置の管理はすべてこちらに委譲
	RootMotionExtractor					rootMotion_;
};

// SkeletonComponent::Awake() / PreUpdate() の遅延定義
inline void SkeletonComponent::Awake()
{
	selfTransform_ = GetOwner()->GetComponent<TransformComponent>();
	// ModelAnimatorComponentを持たない(外部からWorkNodes()を直接
	// 書き換えるだけの)SkeletonComponentも存在しうる(nullptrのままでよい)。
	animator_ = GetOwner()->GetComponent<ModelAnimatorComponent>();
}

inline void SkeletonComponent::PreUpdate(float deltaTime)
{
	if (animator_ != nullptr) {
		animator_->AdvanceFK(deltaTime);
	}
	Finalize();
}