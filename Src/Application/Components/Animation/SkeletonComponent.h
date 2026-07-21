#pragma once
#include "../../Core/Handle.h"

// ============================================================
// SkeletonComponent: 骨格アニメーション(FK)を担当する。
// KdModelWorkを保持し、毎フレームボーンのローカル空間行列を計算する。
//
// 注意: KdModelWorkのm_worldTransformは「モデルローカル空間」
// (ルートノード基準)であり、GameObjectのワールド座標ではない。
// ワールド座標が欲しい場合はTryGetBoneWorldMatrix()を使い、
// 所有者のTransformComponentと合成する。
//
// 既にメッシュ描画用に別のKdModelWorkを持つコンポーネントがある場合は
// 二重計算・アニメのズレを避けるため、そちらではなくこの
// SkeletonComponentが持つKdModelWorkを描画側からも参照すること。
// ============================================================
class SkeletonComponent : public ComponentBase {
public:
	explicit SkeletonComponent(GameObject* owner) : ComponentBase(owner) {}

	void SetModelData(std::string_view fileName) { m_modelWork.SetModelData(fileName); }
	void SetModelData(const std::shared_ptr<KdModelData>& data) { m_modelWork.SetModelData(data); }

	void Start() override {
		selfTransform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void Update(float /*deltaTime*/) override {
		// TODO: アニメーション再生中はここでキー補間して
		// m_modelWork.WorkNodes()内のm_localTransformを書き換える(FKの入力)。
		// FK専用の今の段階では、外部からWorkNodes()を直接書き換えてもらう
		// 想定でも良い。

		if (m_modelWork.NeedCalcNodeMatrices())
		{
			m_modelWork.CalcNodeMatrices();
		}

		// BoneSocketComponent側のキャッシュを無効化するためのバージョン更新
		++m_boneVersion;
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
	const KdModelWork& WorkModel() const { return m_modelWork; }

private:
	KdModelWork m_modelWork;
	TransformComponent* selfTransform_ = nullptr; // 兄弟コンポーネント
	uint32_t m_boneVersion = 0;
};