#pragma once
#include "../../Tags/IModelRenderSource.h"
#include "../../Tags/IAnimationPostProcess.h"

class ModelAnimatorComponent;

// モデルを保持し、アニメーターのオーケストレーターとして振舞う
class SkeletonComponent : public ComponentBase, public IModelRenderSource {
public:
	explicit SkeletonComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetModelData(std::string_view fileName) { modelWork_.SetModelData(KdAssets::Instance().m_modeldatas.GetData(fileName)); }
	void SetModelData(const std::shared_ptr<KdModelData>& data) { modelWork_.SetModelData(data); }

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
	KdModelWork modelWork_;
	TransformComponent* selfTransform_ = nullptr; // 兄弟コンポーネント
	ModelAnimatorComponent* animator_ = nullptr;   // 兄弟コンポーネント(付いていなければnullptr)
	uint32_t boneVersion_ = 0;
};