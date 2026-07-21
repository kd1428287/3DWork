#pragma once
#include "../Transform/TransformComponent.h"

class ModelRenderComponent : public ComponentBase, public IRenderable {
public:
	ModelRenderComponent(GameObject* owner, std::string modelName)
		: ComponentBase(owner)
	{
		spModel_ = std::make_shared<KdModelWork>(
			KdAssets::Instance().m_modeldatas.GetData(modelName)
		);
	}

	// モデルをセットする
	void SetModel(std::string modelName)
	{
		spModel_ = std::make_shared<KdModelWork>(
			KdAssets::Instance().m_modeldatas.GetData(modelName)
		);
	}
	void SetModel(const std::shared_ptr<KdModelData>&model)
	{
		spModel_ = std::make_shared<KdModelWork>(model);
	}
	void SetModel(const std::shared_ptr<KdModelWork>&model)
	{
		spModel_ = model;
	}

	void Start()override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// 光を遮るオブジェクト(影を落とす側)としてシャドウマップに描く
	void GenerateDepthMapFromLight() override	{ if (layer_ & RenderLayer::GenerateDepthMapFromLight)DrawModel(); }
	// 陰影のないオブジェクト(背景など)
	void DrawUnLit() override					{ if (layer_ & RenderLayer::DrawUnLit)DrawModel(); }
	// 陰影のあるオブジェクト(光源の影響を受ける)
	void DrawLit() override						{ if (layer_ & RenderLayer::DrawLit)DrawModel(); }
	// エフェクト(陰影なし)
	void DrawEffect() override					{ if (layer_ & RenderLayer::DrawEffect)DrawModel(); }
	// 自ら光るオブジェクト・ブルーム対象
	void DrawBright() override					{ if (layer_ & RenderLayer::DrawBright)DrawModel(); }
	// 2Dスプライト
	void DrawSprite() override					{ if (layer_ & RenderLayer::DrawSprite)DrawModel(); }
	// デバッグ描画
	void DrawDebug() override					{ if (layer_ & RenderLayer::DrawDebug)DrawModel(); }

	void DrawModel() {
		if (!spModel_ || !transform_)return;
		KdShaderManager::Instance().m_StandardShader.DrawModel(*spModel_, transform_->GetWorldMatrix());
	}
	
	void SetLayer(const uint8_t& layer) { layer_ = layer; }

	// 別コンポーネント(例: ModelAnimatorComponent)から、同じKdModelWorkを
	// 共有して参照できるようにするためのゲッター
	const std::shared_ptr<KdModelWork>& GetModel() const { return spModel_; }

private:
	TransformComponent* transform_ = nullptr;
	std::shared_ptr<KdModelWork> spModel_ = nullptr;
	uint8_t layer_ = RenderLayer::DrawLit | RenderLayer::GenerateDepthMapFromLight;
};