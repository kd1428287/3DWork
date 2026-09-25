#pragma once
#include "../../Tags/IModelRenderSource.h"
#include "../../Tags/IRenderable.h"

// Prefab/JSONから受け取る設定(paramsのキー名はメンバ名と同じ)
struct OutlineConfig
{
	float width = 0.02f;	// 輪郭線の太さ(ワールド単位)
};

// ============================================================
// 同一GameObjectのモデル(IModelRenderSource)を、裏面拡大法の輪郭線として
// もう1回描画する。ModelRenderComponentとは独立して動くため、付けた
// オブジェクトだけがコストを払う。墨色への変換はPS_Lit側で行う。
// ============================================================
class OutlineRenderComponent : public ComponentBase, public IRenderable
{
public:
	using Config = OutlineConfig;

	explicit OutlineRenderComponent(GameObject* owner);

	void Awake() override;

	void SetConfig(const Config& config) { width_ = config.width; }

	// 陰影ありパスで、前面カリングの輪郭描画を行う
	void DrawLit() override;

private:
	TransformComponent* transform_ = nullptr;
	IModelRenderSource* modelSource_ = nullptr;

	float width_ = 0.02f;
};
