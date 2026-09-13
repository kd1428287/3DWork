#pragma once

// ============================================================
// ModelRenderComponentが描画直前に呼び出す、個別の描画パラメータ
// 適用フック。
//
// ForceMaxDepthやスカイドームのエッジフェードのように「一部の
// オブジェクトにしか要らない」演出パラメータを、ModelRenderComponent
// 自体に持たせると、汎用性の薄いフィールドで肥大化してしまう。
// そこで効果ごとに専用の小さいコンポーネントとして実装し、
// TAG_INTERFACES経由でModelRenderComponentが収集して適用する形にする。
//
// このタグを実装していないGameObjectは一切コストを払わない
// (ModelRenderComponent::modifiers_に乗らないため、Apply()自体が
//  呼ばれない)。
//
// 使い方:
//   class ForceMaxDepthComponent : public ComponentBase, public IRenderStateModifier
// のように多重継承し、Apply()だけ実装する。
// ============================================================

class KdStandardShader;

class IRenderStateModifier
{
public:
	virtual ~IRenderStateModifier() = default;

	// ModelRenderComponent::DrawModel()内、DrawModel()本体の呼び出し直前に
	// (Start()時に収集された全modifierの数だけ)呼ばれる。
	// ここでシェーダー側のSetXxx()を呼び、対応する定数バッファへ値を書き込む。
	virtual void Apply(KdStandardShader& shader) const = 0;
};
