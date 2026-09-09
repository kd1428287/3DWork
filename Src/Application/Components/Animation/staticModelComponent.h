//#pragma once
//#include "../Tags/IModelRenderSource.h"
//
//// アニメーションしない静的モデル(スカイドーム・背景等)を保持し、
//// SkeletonComponentと同じIModelRenderSource経由でModelRenderComponentに供給する。
//// KdModelWork自体はアニメーションしないモデルも扱えるため、専用の描画経路は不要。
//class StaticModelComponent : public ComponentBase, public IModelRenderSource
//{
//public:
//	StaticModelComponent(GameObject* owner) : ComponentBase(owner) {}
//
//	void SetModelData(const std::shared_ptr<KdModelData>& data)
//	{
//		model_.SetData(data); // ※KdModelWork側の実際のsetter名は要確認
//	}
//
//	KdModelWork* GetModel() override { return &model_; }
//	bool IsModelDrawable() const override { return model_.GetData() != nullptr; }
//
//private:
//	KdModelData model_; // アニメーション更新(CalcNodeMatrices等)は呼ばれない＝静的のまま
//};