#pragma once
#include "../../Tags/IModelRenderSource.h"
#include "../../Tags/IAnimationPostProcess.h"

class ModelAnimatorComponent;

// 初期設定(Prefab/JSONから渡す値)。
struct SkeletonConfig
{
	std::string model;
	std::string animations;
};


// モデルを保持し、アニメーターのオーケストレーターとして振舞う
class SkeletonComponent : public ComponentBase, public IModelRenderSource {
public:
	using Config = SkeletonConfig;

	explicit SkeletonComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetConfig(const Config& config)
	{
		SetModelData(config.model);
		AddAnimationDirectory(config.animations);
	}

	void SetModelData(std::string_view fileName) { SetModelData(KdAssets::Instance().m_modeldatas.GetData(fileName)); }
	void SetModelData(const std::shared_ptr<KdModelData>& data)
	{
		modelWork_.SetModelData(data);
		ApplyPendingAnimSources();
	}

	// アニメーション専用glTFを登録する(モデル未設定ならSetModelData時に適用)
	void AddAnimationFile(std::string_view filePath) { AddAnimationSource(filePath, false); }
	// フォルダ内のアニメーション専用glTFを一括登録する(同上)
	void AddAnimationDirectory(std::string_view dirPath) { AddAnimationSource(dirPath, true); }

	void Awake() override;
	void PreUpdate(float deltaTime) override;

	void PostUpdate(float deltaTime) override {
		for (IAnimationPostProcess* postProcess : GetOwner()->GetTagged<IAnimationPostProcess>()) {
			postProcess->SolveIK();
		}
		Finalize();
	}

	void Finalize() {
		if (modelWork_.NeedCalcNodeMatrices())
		{
			modelWork_.CalcNodeMatrices();
			++boneVersion_;
		}
	}

	// ボーンのモデルローカル空間行列
	bool TryGetBoneLocalMatrix(std::string_view boneName, Math::Matrix& outMatrix) const
	{
		const KdModelWork::Node* node = modelWork_.FindNode(boneName);
		if (node == nullptr) { return false; }
		outMatrix = node->m_worldTransform;
		return true;
	}

	// ボーンのワールド空間行列(所有者のTransformComponentと合成)
	bool TryGetBoneWorldMatrix(std::string_view boneName, Math::Matrix& outMatrix) const
	{
		Math::Matrix localMat;
		if (!TryGetBoneLocalMatrix(boneName, localMat)) { return false; }

		// スケールの二重伝播を避けるためUnscaled行列を使う
		Math::Matrix ownerMat = selfTransform_ ? selfTransform_->GetUnscaledMatrix() : Math::Matrix::Identity;

		outMatrix = localMat * ownerMat;
		return true;
	}

	uint32_t GetBoneVersion() const { return boneVersion_; }

	KdModelWork& WorkModel() { return modelWork_; }

	KdModelWork* GetModel() override { return &modelWork_; }

	bool IsModelDrawable() const override { return true; }

private:
	struct AnimationSource
	{
		std::string	path;
		bool		isDirectory;
	};

	void AddAnimationSource(std::string_view path, bool isDirectory)
	{
		pendingAnimSources_.push_back({ std::string(path), isDirectory });
		ApplyPendingAnimSources();
	}

	// モデルが設定済みなら、溜まっている登録を適用する
	void ApplyPendingAnimSources()
	{
		auto spData = modelWork_.GetData();
		if (!spData) { return; }

		for (const auto& src : pendingAnimSources_)
		{
			if (src.isDirectory) { spData->LoadAnimationDirectory(src.path); }
			else { spData->LoadAnimationFile(src.path); }
		}
		pendingAnimSources_.clear();
	}

	KdModelWork modelWork_;
	std::vector<AnimationSource> pendingAnimSources_; // モデル設定前に登録された分
	TransformComponent* selfTransform_ = nullptr; // 兄弟コンポーネント
	ModelAnimatorComponent* animator_ = nullptr;   // 兄弟コンポーネント(付いていなければnullptr)
	uint32_t boneVersion_ = 0;
};