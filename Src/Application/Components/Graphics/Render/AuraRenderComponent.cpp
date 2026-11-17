#include "AuraRenderComponent.h"

AuraRenderComponent::AuraRenderComponent(GameObject* owner)
	: ComponentBase(owner)
{
	// 個体ごとに1〜255を割り当てる(隣接するキャラ同士で消し合わないように)
	static UINT counter = 0;
	stencilRef_ = (counter++ % 255) + 1;
}

// 同一GameObjectのTransformとモデル供給者を収集する(OutlineRenderComponentと同じ方式)
void AuraRenderComponent::Awake()
{
	transform_ = GetOwner()->GetComponent<TransformComponent>();

	auto sources = GetOwner()->GetTagged<IModelRenderSource>();
	modelSource_ = sources.empty() ? nullptr : sources.front();
}

void AuraRenderComponent::Update(float deltaTime)
{
	time_ += deltaTime;
}

void AuraRenderComponent::SetConfig(const Config& config)
{
	config_ = config;
	config_.layers = std::clamp(config_.layers, 1, 3);

	noiseTex_ = KdAssets::Instance().m_textures.GetData(config_.noiseTexturePath);
}

// 膨らませたモデルを層ごとに強さ・太さ・ノイズを変えて加算で重ねる
void AuraRenderComponent::DrawEffect()
{
	if (config_.intensity <= 0.0f || !transform_ || !modelSource_) { return; }
	if (!modelSource_->IsModelDrawable()) { return; }

	KdModelWork* model = modelSource_->GetModel();
	if (!model) { return; }

	KdShaderManager& manager = KdShaderManager::Instance();
	KdStandardShader& shader = manager.m_StandardShader;

	// パーティクル(GPUParticleShader::Draw)と同じ基準でブレンドを選ぶ
	const bool effectLayer = manager.m_postProcessShader.IsEffectLayerActive();

	const Math::Matrix& world = transform_->GetWorldMatrix();
	const float baseY = world.Translation().y;	// 炎の根元は足元(オブジェクト原点)

	shader.BeginAura();

	// シルエットをステンシルに書く(色なし・膨らませない)
	{
		KdAuraStyle silhouette;
		silhouette.Kind = (int)KdAuraKind::Rim;
		silhouette.Intensity = 1.0f;
		silhouette.Width = 0.0f;
		silhouette.Rise = 0.0f;
		silhouette.BaseY = baseY;

		manager.ChangeBlendState(KdBlendState::NoColorWrite);
		manager.ChangeDepthStencilState(KdDepthStencilState::StencilWrite, stencilRef_);
		shader.SetAuraStyle(silhouette);
		shader.DrawModel(*model, world);
		manager.UndoDepthStencilState();
		manager.UndoBlendState();
	}

	//manager.ChangeBlendState(effectLayer ? KdBlendState::EffectAdd : KdBlendState::Add);
	manager.ChangeBlendState(KdBlendState::Alpha);
	// シルエットの外側だけに描く(深度は読み取りのみ)
	manager.ChangeDepthStencilState(KdDepthStencilState::StencilOutline, stencilRef_);
	const bool isInk = (config_.kind == KdAuraKind::Ink);

	// 墨型は体の後ろに回り込む背面だけを描く
	//manager.ChangeRasterizerState(isInk ? KdRasterizerState::CullFront : KdRasterizerState::CullBack);
	manager.ChangeRasterizerState(KdRasterizerState::CullFront);

	if (noiseTex_) { shader.SetAuraNoiseTexture(*noiseTex_); }

	const bool lifts = (config_.kind != KdAuraKind::Rim);

	for (int i = 0; i < config_.layers; ++i)
	{
		KdAuraStyle style;
		style.Kind = (int)config_.kind;
		style.Color = config_.color;
		style.CoreColor = config_.coreColor;
		style.Intensity = config_.intensity * powf(0.55f, (float)i);	// 外側の層ほど薄く
		style.Width = config_.width * (1.0f + 0.75f * i);
		style.FresnelPower = config_.fresnelPower;
		style.NoiseScale = config_.noiseScale * (1.0f + 0.5f * i);
		style.ScrollSpeed = config_.scrollSpeed;
		style.Time = time_ + 1.7f * i;	// 層ごとにゆらぎをずらす
		style.Rise = lifts ? config_.rise * (1.0f + 0.5f * i) : 0.0f;	// リム型は持ち上げない
		style.BaseY = baseY;
		style.Height = config_.height;
		style.Softness = config_.softness;

		// 描画後に自動で初期値へ戻るので、層ごとに設定し直す
		if (isInk) { shader.SetInkStyle(KdInkStyle::FromPreset(config_.inkPreset)); }
		shader.SetAuraStyle(style);
		shader.DrawModel(*model, world);
	}

	manager.UndoRasterizerState();
	manager.UndoDepthStencilState();
	manager.UndoBlendState();
	shader.EndAura();
}