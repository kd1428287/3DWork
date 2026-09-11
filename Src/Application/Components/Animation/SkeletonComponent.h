#pragma once
#include "../../Core/Handle.h"
#include "../Render/IModelRenderSource.h"
#include "IAnimationPostProcess.h"

class ModelAnimatorComponent;

// モデルを保持し、アニメーターのオーケストレーターとして振舞う
class SkeletonComponent : public ComponentBase, public IModelRenderSource {
public:
	explicit SkeletonComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetModelData(std::string_view fileName) { m_modelWork.SetModelData(KdAssets::Instance().m_modeldatas.GetData(fileName)); }
	void SetModelData(const std::shared_ptr<KdModelData>& data) { m_modelWork.SetModelData(data); }

	void Start() override;
	void PreUpdate(float deltaTime) override;

	void PostUpdate(float deltaTime) override {
		for (IAnimationPostProcess* postProcess : GetOwner()->GetTagged<IAnimationPostProcess>()) {
			postProcess->SolveIK();
		}
		Finalize();
	}

	void Finalize() {
		if (m_modelWork.NeedCalcNodeMatrices())
		{
			m_modelWork.CalcNodeMatrices();
			++m_boneVersion;
		}
	}

	// ボーンのモデルローカル空間行列
	bool TryGetBoneLocalMatrix(std::string_view boneName, Math::Matrix& outMatrix) const
	{
		const KdModelWork::Node* node = m_modelWork.FindNode(boneName);
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

	uint32_t GetBoneVersion() const { return m_boneVersion; }

	KdModelWork& WorkModel() { return m_modelWork; }

	KdModelWork* GetModel() override { return &m_modelWork; }
	// 必要なら: モデル未ロード時はfalseを返すよう調整
	bool IsModelDrawable() const override { return true; }

private:
	KdModelWork m_modelWork;
	TransformComponent* selfTransform_ = nullptr; // 兄弟コンポーネント
	ModelAnimatorComponent* animator_ = nullptr;   // 兄弟コンポーネント(付いていなければnullptr)
	uint32_t m_boneVersion = 0;
};