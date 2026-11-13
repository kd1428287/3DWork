#pragma once
#include "../../../Tags/IRenderStateModifier.h"

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
