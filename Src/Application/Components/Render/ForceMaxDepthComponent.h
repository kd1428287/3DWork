#pragma once
#include "IRenderStateModifier.h"

// ============================================================
// このGameObjectに付けると、描画時にKdStandardShaderの
// ForceMaxDepthを有効化する(頂点シェーダー側でZを最奥(1.0)に
// 固定する)。スカイドームのように、常に他の全ての不透明
// オブジェクトより奥に描画したいモデルに使う。
//
// ※ ChangeDepthStencilState(ZWriteDisable)等、深度バッファへの
//   書き込み有無の制御とは別軸の機能。深度バッファを汚したく
//   ない場合は、合わせてModelRenderComponent側の描画パス
//   (RenderLayer)の設定と組み合わせて使うこと。
// ============================================================
class ForceMaxDepthComponent : public ComponentBase, public IRenderStateModifier
{
public:
	explicit ForceMaxDepthComponent(GameObject* owner);

	void Apply(KdStandardShader& shader) const override;
};
