#pragma once
#include "../Transform/TransformComponent.h"
#include "IModelRenderSource.h"
#include "IRenderStateModifier.h"
#include "IRenderable.h"

// ============================================================
// 同一GameObject上のIModelRenderSource(SkeletonComponent等)から
// モデルを取得し、RenderLayerで指定された描画パスで描画する。
//
// モデル単位の個別演出(ForceMaxDepth、スカイドームのエッジフェード等)は
// このクラス自身は一切知らない。それぞれ専用のIRenderStateModifier実装
// コンポーネント(ForceMaxDepthComponent、SkyDomeEdgeFadeComponent等)を
// 同じGameObjectにAddComponentすることで、Start()時に自動的に収集され、
// 描画直前(DrawModel()の中、実際のDrawModel呼び出し直前)に適用される。
//
// 新しい演出効果が増えても、このクラス自体は変更不要(オープン・
// クローズド原則)。増えるのは「新しいIRenderStateModifier実装1個」
// だけで済む。
// ============================================================
class ModelRenderComponent : public ComponentBase, public IRenderable
{
public:
	explicit ModelRenderComponent(GameObject* owner);

	void Start() override;

	// 光を遮るオブジェクト(影を落とす側)としてシャドウマップに描く
	void GenerateDepthMapFromLight() override;
	// 陰影のないオブジェクト(背景など)
	void DrawUnLit() override;
	// 陰影のあるオブジェクト(光源の影響を受ける)
	void DrawLit() override;
	// エフェクト(陰影なし)
	void DrawEffect() override;
	// 自ら光るオブジェクト・ブルーム対象
	void DrawBright() override;
	// 2Dスプライト
	void DrawSprite() override;
	// デバッグ描画
	void DrawDebug() override;

	// RenderLayer(ビットの組み合わせ。例: RenderLayer::DrawLit | RenderLayer::GenerateDepthMapFromLight)
	void SetLayer(const uint8_t& layer) { layer_ = layer; }

private:
	// 実際の描画本体：dispatch(各DrawXxx)からのみ呼ばれる。
	// パス判定には一切関与しない。
	void DrawModel();

	TransformComponent* transform_ = nullptr;
	IModelRenderSource* modelSource_ = nullptr;

	// Start()時に一度だけGetTagged<IRenderStateModifier>()で収集し、
	// 以後は毎フレーム参照するだけ(タグレジストリの再検索はしない)。
	std::vector<IRenderStateModifier*> modifiers_;

	uint8_t layer_ = RenderLayer::DrawLit | RenderLayer::GenerateDepthMapFromLight;
};