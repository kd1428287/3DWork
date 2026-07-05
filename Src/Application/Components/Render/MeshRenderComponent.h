#pragma once
#include <cstdio>
#include <string>
#include <utility>

// ============================================================
// サンプル描画コンポーネント: 陰影のある3Dメッシュを描画する想定。
// 実際にはここで頂点バッファのバインドやシェーダー呼び出しを行う。
// ここではデモのため、呼ばれたことをprintfで示すだけにしている。
//
// ComponentBase(ライフサイクル) と IRenderable(描画) を多重継承し、
// 描画に必要なメソッドだけをoverrideする。
// ============================================================
class MeshRenderComponent : public ComponentBase, public IRenderable {
public:
	MeshRenderComponent(GameObject* owner, std::string meshName)
		: ComponentBase(owner), meshName_(std::move(meshName)) {
	}

	void GenerateDepthMapFromLight() override {
		std::printf("  [ShadowPass] %s(%s) の影を描く\n",
			GetOwner()->GetName().c_str(), meshName_.c_str());
	}

	void DrawLit() override {
		std::printf("  [LitPass] %s(%s) を陰影付きで描く\n",
			GetOwner()->GetName().c_str(), meshName_.c_str());
	}

private:
	std::string meshName_;
};