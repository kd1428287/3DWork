#pragma once
#include "IRenderStateModifier.h"

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
class SkyDomeEdgeFadeComponent : public ComponentBase, public IRenderStateModifier
{
public:
	explicit SkyDomeEdgeFadeComponent(GameObject* owner);

	// bottomY : このワールドY以下で完全に透明
	// topY    : このワールドY以上で完全に不透明
	void SetFadeRange(float bottomY, float topY);

	void Apply(KdStandardShader& shader) const override;

private:
	float bottomY_ = 0.0f;
	float topY_ = 0.0f;
};
