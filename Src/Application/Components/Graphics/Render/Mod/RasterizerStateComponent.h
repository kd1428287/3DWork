#pragma once
#include "../../../Tags/IRenderStateModifier.h"

// 描画中だけラスタライザステートを切り替えるModコンポーネント。
// ステートはKdShaderManager側の管理なので、shader引数は使わない。
class RasterizerStateComponent : public ComponentBase, public IRenderStateModifier
{
public:
	RasterizerStateComponent(GameObject* owner, KdRasterizerState state)
		: ComponentBase(owner), state_(state) {}

	void Apply(KdStandardShader&) const override {
		KdShaderManager::Instance().ChangeRasterizerState(state_);
	}
	// Change/Undoはスタック対応のため、必ず1対1で呼ばれる必要がある
	void Restore(KdStandardShader&) const override {
		KdShaderManager::Instance().UndoRasterizerState();
	}

private:
	KdRasterizerState state_;
};
