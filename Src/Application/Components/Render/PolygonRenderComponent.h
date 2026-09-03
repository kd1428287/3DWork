#pragma once
#include "../Tags/IPolygonRenderSource.h"

// ============================================================
// KdPolygon系の描画のみを担当する汎用レンダラー。
// ModelRenderComponentとは完全に独立した、ポリゴン専用の描画コンポーネント。
//
// 自前でリストを持たず、毎回GetOwner()->GetTagged<IPolygonRenderSource>()を
// 使って「同じGameObject上でIPolygonRenderSourceを実装している
// コンポーネント」をGameObjectのタグレジストリから取得する。
// 登録/解除はGameObject::AddComponent/RemoveComponent側で自動的に
// 行われるため、このクラスはリストの整合性を一切気にしなくてよい
// (前回案のAddMaterial/RemoveMaterialのような手動管理は不要)。
//
// 1つのGameObjectに複数のポリゴン系素材が乗っていても、それらを
// まとめて描画できる。将来別のポリゴン系エフェクト
// (仮称: RibbonPolygonComponent等)が増えても、そちらが
// IPolygonRenderSourceを実装するだけで、このクラスは無改造で使える。
// ============================================================
class PolygonRenderComponent : public ComponentBase, public IRenderable {
public:
	explicit PolygonRenderComponent(GameObject* owner)
		: ComponentBase(owner) {}

	void Start()override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}
	void GenerateDepthMapFromLight() override	{ if (layer_ & RenderLayer::GenerateDepthMapFromLight)DrawAll(); }
	void DrawUnLit() override					{ if (layer_ & RenderLayer::DrawUnLit)DrawAll(); }
	void DrawLit() override						{ if (layer_ & RenderLayer::DrawLit)DrawAll(); }
	void DrawEffect() override					{ if (layer_ & RenderLayer::DrawEffect)DrawAll(); }
	void DrawBright() override					{ if (layer_ & RenderLayer::DrawBright)DrawAll(); }
	void DrawSprite() override					{ if (layer_ & RenderLayer::DrawSprite)DrawAll(); }
	void DrawDebug() override					{ if (layer_ & RenderLayer::DrawDebug)DrawAll(); }

	// ポリゴン系エフェクトは陰影なし表現が多いため、
	// ModelRenderComponentのデフォルト(DrawLit)とは変え、
	// DrawEffectのみをデフォルトにしている。
	void SetLayer(const uint8_t& layer) { layer_ = layer; }

private:
	void DrawAll()
	{
		// GetTagged<Tag>()はGameObjectのタグレジストリから毎回取得する
		// (IsEnabled()なコンポーネントのみが対象。GameObject.h参照)。
		for (IPolygonRenderSource* source : GetOwner()->GetTagged<IPolygonRenderSource>()) {
			if (!source->IsPolygonDrawable()) { continue; }

			KdPolygon* polygon = source->GetPolygon();
			if (!polygon) { continue; }

			// TODO: 実際のKdPolygon側の描画API名に置き換える。
			// (例: KdShaderManager::Instance().m_〇〇Shader.DrawPolygon(*polygon) 等、
			//  ModelRenderComponent::DrawModel()のm_StandardShader.DrawModel()に相当するもの)
			KdShaderManager::Instance().m_StandardShader.DrawPolygon(*polygon/*, transform_->GetUnscaledMatrix()*/);

			
		}
	}

	TransformComponent* transform_ = nullptr;
	uint8_t layer_ = RenderLayer::DrawEffect;
};