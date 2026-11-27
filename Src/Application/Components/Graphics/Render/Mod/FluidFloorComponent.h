#pragma once
#include "../../../Tags/IRenderStateModifier.h"
#include "../../../Tags/IRenderable.h"
#include "Framework/Shader/PostProcessShader/FluidSimulator.h"	// includeパスは配置に合わせて調整

// 床の墨の設定。ワールドXZの範囲と、床の高さ等(未指定のキーは初期値)
struct FluidFloorConfig
{
	float originX = 0.0f;		// 流体マップのワールドXZ最小値
	float originZ = 0.0f;
	float sizeX = 100.0f;		// 流体マップのワールドXZの大きさ
	float sizeZ = 100.0f;
	float floorY = 0.0f;		// 床の高さ
	float floorBand = 0.05f;	// 床とみなす高さの幅
	float absorb = 3.5f;		// 墨の濃さ
	int   simWidth = 256;		// 流体の格子の幅
};

// 付けたGameObject(ステージ)の床へ、墨の流体を描く。流体の更新(PreDraw)と、Litパス中だけの適用(Apply/Restore)を担う。
// 同じGameObjectのModelRenderComponentより前にAddComponentすること(modifiers_に収集されるため)。
class FluidFloorComponent : public ComponentBase, public IRenderable, public IRenderStateModifier
{
public:
	FluidFloorComponent(GameObject* owner, const FluidFloorConfig& config)
		: ComponentBase(owner), simWidth_(config.simWidth)
	{
		params_.MapOrigin = { config.originX, config.originZ };
		params_.MapSize = { config.sizeX, config.sizeZ };
		params_.Absorb = config.absorb;
		params_.FloorY = config.floorY;
		params_.FloorBand = config.floorBand;
	}

	void Awake() override
	{
		assert(params_.MapSize.x > 0.0f && params_.MapSize.y > 0.0f);

		// マップの縦横比に合わせると、テクセルがワールドで正方形になる
		fluid_.Init(simWidth_, params_.MapSize.x / params_.MapSize.y);
	}

	// Stepは描画フェーズでしか呼べないため、ここでは経過時間を溜めるだけ
	void PostUpdate(float dt) override { dt_ += dt; }

	// 全ての描画より前に、流体を1フレーム進める
	void PreDraw() override
	{
		fluid_.Step(dt_);
		dt_ = 0.0f;
		density_ = fluid_.GetDensityTexture();
	}

	void Apply(KdStandardShader& shader) const override
	{
		if (density_) { shader.SetFluidFloor(*density_, params_); }
	}

	void Restore(KdStandardShader& shader) const override { shader.ClearFluidFloor(); }

private:
	KdStandardShader::cbFluidFloor params_;
	int simWidth_ = 256;

	FluidSimulator fluid_;
	std::shared_ptr<KdTexture> density_;	// 今フレームの読み側(Stepのたびに入れ替わる)
	float dt_ = 0.0f;						// 前回のStepからの経過時間
};