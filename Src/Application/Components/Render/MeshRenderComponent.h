#pragma once
#include <cstdio>
#include <string>
#include <utility>
#include "../Transform/TransformComponent.h"

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

		TransformComponent* trans = GetOwner()->GetComponent<TransformComponent>();
		Math::Matrix mat =
			Math::Matrix::CreateScale(trans->scale) *
			Math::Matrix::CreateFromYawPitchRoll(trans->rotation) *
			Math::Matrix::CreateTranslation(trans->position);
		KdShaderManager::Instance().m_StandardShader.DrawModel(
			*KdAssets::Instance().m_modeldatas.GetData(meshName_),
			mat
		);
	}

	void DrawLit() override {
		std::printf("  [LitPass] %s(%s) を陰影付きで描く\n",
			GetOwner()->GetName().c_str(), meshName_.c_str());
	}

private:
	std::string meshName_;
};