#pragma once
#include "SkeletonComponent.h"
#include "RootMotionExtractor.h"

// モデルのボーンアニメーション再生
class ModelAnimatorComponent : public ComponentBase
{
public:
	explicit ModelAnimatorComponent(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override
	{
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
	}

	void Start() override
	{
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
	}

	// アニメーション名を指定して再生開始
	// ・animName				… アニメーション名
	// ・loop					… ループ再生するか
	// ・targetDurationSeconds	… 指定した場合、クリップの実際の長さ(m_maxLength)に
	//   関わらず、ちょうどこの秒数で再生し終わるよう再生速度を自動スケーリングする
	void Play(std::string_view animName, bool loop = true, float targetDurationSeconds = -1.0f)
	{
		if (!skeleton_) { return; }

		auto animData = skeleton_->WorkModel().GetAnimation(animName);
		if (!animData) {
			assert(0 && "ModelAnimatorComponent ファイルが存在しません。ファイルパスを確認してください");
			return;
		}

		// 既に同じアニメーションを再生中なら再生しない
		if (spNowPlaying_ == animData) { return; }

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

		spNowPlaying_ = animData;
		animator_.SetAnimation(animData, loop);

		// ルートモーション抽出中に別アニメーションへ切り替わった場合、
		// 旧アニメーションの最終位置と新アニメーションの先頭位置の差分を
		// 1フレームの移動量として誤って計上しないよう、基準位置を
		// 次のAdvanceFK()の冒頭で取り直す
		rootMotion_.NotifyAnimationChanged();

		if (targetDurationSeconds > 0.0f && animData->m_maxLength > 0.0f) {
			targetSpeedOverride_ = animData->m_maxLength / targetDurationSeconds;
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
		const float speed = (targetSpeedOverride_ > 0.0f) ? targetSpeedOverride_ : m_fps;
		const float timeBeforeAdvance = animator_.GetTime();
		animator_.AdvanceTime(skeleton_->WorkModel().WorkNodes(), deltaTime * speed);
		rootMotion_.FinalizeFrame(timeBeforeAdvance, animator_.GetTime());

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

	// --- ルートモーション ---------------------------------------------
	// Inplaceではない(位置移動が焼き込まれた)アニメーションを使いたい場合、
	// 抽出元にするボーン名(通常はHip/Root)を指定する。空文字を渡すと無効化。
	void SetRootMotionBoneName(std::string_view name) { rootMotion_.SetBoneName(name); }

	// 抽出したデルタに掛ける倍率
	void SetRootMotionScale(float scale) { rootMotion_.SetUnitScale(scale); }

	// ボーンのローカル空間で「前後」「左右」に対応する軸を指定する
	void SetRootMotionForwardAxis(RootMotionAxis axis, float sign = 1.0f) { rootMotion_.SetForwardAxis(axis, sign); }
	void SetRootMotionRightAxis(RootMotionAxis axis, float sign = 1.0f) { rootMotion_.SetRightAxis(axis, sign); }
	void SetRootMotionExtractRotation(bool enabled) { rootMotion_.SetExtractRotation(enabled); }

	// このフレームで蓄積されたルートモーションの移動量　呼ぶと消費される
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

	float								m_fps = 60.0f;

	// Play()にtargetDurationSecondsが渡された場合、m_fpsの代わりに使う
	float								targetSpeedOverride_ = -1.0f;

	std::vector<Math::Matrix>			blendFromLocalTransforms_; // 切り替わる直前の全ボーンのローカル行列
	float								blendDuration_ = 0.15f;    // ブレンドにかける時間(秒)
	float								blendElapsed_ = 0.0f;      // ブレンド開始からの経過時間(blendDuration_以上ならブレンド終了)

	
	// ボーン解決・巻き戻り検知・基準位置の管理はすべてこちらに委譲
	RootMotionExtractor					rootMotion_;
};

// SkeletonComponent::Start() / PreUpdate() の遅延定義
inline void SkeletonComponent::Start()
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