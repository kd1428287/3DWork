#pragma once
#include "../../../Tags/IRenderStateModifier.h"

// ============================================================
// スカイドーム(半球)の下端(フチ)を、下るにつれて透明にするための
// 描画パラメータをKdStandardShaderへ渡す。
//
// bottomY/topYはワールド座標のY値。bottomY以下は完全に透明、
// topY以上は完全に不透明になるよう、PS側でワールド座標のYから
// アルファへ線形補間する(実際の減衰計算はシェーダー側で行う。
// このコンポーネントはパラメータを渡すだけ)。
//
// このコンポーネントを持たないGameObjectは一切コストを払わない
// (Apply()自体が呼ばれない)。
//
// ※ このフェードはアルファブレンドが有効な状態(KdBlendState::Alpha)
//   でなければ画面に反映されない点に注意。ブレンドステートの
//   切り替えはこのコンポーネントの責務外。
// ============================================================
class InkStyleComponent : public ComponentBase, public IRenderStateModifier
{
public:
	explicit InkStyleComponent(GameObject* owner, KdInkStyle style) 
		: ComponentBase(owner),style_(style) {};

	void Apply(KdStandardShader& shader) const override { shader.SetInkStyle(style_); }

	void SetInkStyle(KdInkStyle style) { style_ = style; }

private:
	KdInkStyle style_ = KdInkStyle::Bright();
};
