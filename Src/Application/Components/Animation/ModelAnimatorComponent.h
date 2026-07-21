#pragma once
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
	// ・animName	… KdModelData内のアニメーション名(glTFのアニメーションクリップ名)
	// ・loop		… ループ再生するか
	void Play(std::string_view animName, bool loop = true)
	{
		if (!skeleton_) { return; }

		auto animData = skeleton_->WorkModel().GetAnimation(animName);
		if (!animData) { 
			assert(0 && "ModelAnimatorComponent ファイルが存在しません。ファイルパスを確認してください");
			return; }

		// 既に同じアニメーションを再生中なら、頭出ししない
		// (毎フレームPlay()を呼ぶような使い方をしても、再生位置が
		//  リセットされ続けてしまわないようにするための簡易ガード)
		if (spNowPlaying_ == animData) { return; }

		spNowPlaying_ = animData;
		animator_.SetAnimation(animData, loop);
	}

	// 再生速度をfps換算で指定して毎フレーム進行させる
	// ・fps		… 1秒間に進めるアニメーションのフレーム数
	//   ※ KdGLTFLoader側でアニメーションキーの時間を「秒」で読み込んでいるか
	//     「フレーム数」で読み込んでいるかによって、ここで渡すべき値の意味が
	//     変わる。もし秒単位ならfps引数には単純にdeltaTimeを渡すこと。
	void Update(float deltaTime) override
	{
		if (!skeleton_) { return; }

		animator_.AdvanceTime(skeleton_->WorkModel().WorkNodes(), deltaTime * m_fps);
	}

	// 再生速度の基準fpsを設定(デフォルト30fps)
	void SetFPS(float fps) { m_fps = fps; }

	// 現在のアニメーションが最後まで再生し終わったか(ループ再生時は常にfalse)
	bool IsAnimationEnd() const { return animator_.IsAnimationEnd(); }

private:
	SkeletonComponent* skeleton_ = nullptr;

	KdAnimator							animator_;

	// 現在再生中のアニメーションデータ(Play()の多重頭出し防止用)
	std::shared_ptr<KdAnimationData>	spNowPlaying_ = nullptr;

	float								m_fps = 30.0f;
};
