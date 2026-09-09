#pragma once
#include "../Transform/TransformComponent.h"
#include "../Tags/IModelRenderSource.h"

class ModelRenderComponent : public ComponentBase, public IRenderable
{
public:
	ModelRenderComponent(GameObject* owner)
		: ComponentBase(owner) {}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// 同一GameObject上のモデル供給者を探す(SkeletonComponent or StaticModelComponent等)
		auto sources = GetOwner()->GetTagged<IModelRenderSource>();
		modelSource_ = sources.empty() ? nullptr : sources.front();

		// 想定外(モデル供給者が複数いる)の場合は設定ミスの可能性が高いので検知しておく
		assert(sources.size() <= 1 && "ModelRenderComponent: 複数のIModelRenderSourceが見つかりました");
	}

	void GenerateDepthMapFromLight() override { if (layer_ & RenderLayer::GenerateDepthMapFromLight) DrawModel(); }
	void DrawUnLit() override { if (layer_ & RenderLayer::DrawUnLit)  DrawModel(); }
	void DrawLit() override { if (layer_ & RenderLayer::DrawLit)    DrawModel(); }
	void DrawEffect() override { if (layer_ & RenderLayer::DrawEffect) DrawModel(); }
	void DrawBright() override { if (layer_ & RenderLayer::DrawBright) DrawModel(); }
	void DrawSprite() override { if (layer_ & RenderLayer::DrawSprite) DrawModel(); }
	void DrawDebug() override { if (layer_ & RenderLayer::DrawDebug)  DrawModel(); }

	void SetLayer(const uint8_t& layer) { layer_ = layer; }

	// cbObject由来の個別オーバーライド(必要な分だけ増やす)
	void SetForceMaxDepth(bool enable) { overrides_.forceMaxDepth = enable; }
	void SetFogEnable(bool enable) { overrides_.fogEnable = enable; }

private:
	struct RenderOverrides
	{
		bool forceMaxDepth = false;
		bool fogEnable = true;
	};

	void DrawModel()
	{
		if (!transform_ || !modelSource_) { return; }
		if (!modelSource_->IsModelDrawable()) { return; }

		KdModelWork* model = modelSource_->GetModel();
		if (!model) { return; }

		ApplyOverrides();

		KdShaderManager::Instance().m_StandardShader.DrawModel(*model, transform_->GetWorldMatrix());
	}

	// overrides_の内容を毎回全項目セットする(前オブジェクトの状態が漏れ残らないように)
	void ApplyOverrides()
	{
		auto& shader = KdShaderManager::Instance().m_StandardShader;
		shader.SetForceMaxDepth(overrides_.forceMaxDepth);
		shader.SetFogEnable(overrides_.fogEnable);
	}

	TransformComponent* transform_ = nullptr;
	IModelRenderSource* modelSource_ = nullptr;

	uint8_t layer_ = RenderLayer::DrawLit | RenderLayer::GenerateDepthMapFromLight;
	RenderOverrides overrides_;
};