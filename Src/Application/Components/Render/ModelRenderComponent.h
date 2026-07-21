#pragma once
#include "../Transform/TransformComponent.h"
#include "../Animation/SkeletonComponent.h"

class ModelRenderComponent : public ComponentBase, public IRenderable {
public:
	ModelRenderComponent(GameObject* owner)
		: ComponentBase(owner){}

	void Start()override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
		skeleton_ = GetOwner()->GetComponent<SkeletonComponent>();
		// model_ = GetOwner()->GetComponent<ModelComponent>();
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
	
	void SetLayer(const uint8_t& layer) { layer_ = layer; }

private:
	void DrawModel() {
		if (!transform_)return;

		if (skeleton_) { KdShaderManager::Instance().m_StandardShader.DrawModel(skeleton_->WorkModel(), transform_->GetWorldMatrix()); }
		//else if (model_) { KdShaderManager::Instance().m_StandardShader.DrawModel(model_->WorkModel(), transform_->GetWorldMatrix()); };
	}

	TransformComponent* transform_ = nullptr;
	SkeletonComponent* skeleton_ = nullptr;
	uint8_t layer_ = RenderLayer::DrawLit | RenderLayer::GenerateDepthMapFromLight;
};