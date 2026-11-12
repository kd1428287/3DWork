#include "AuraRenderComponent.h"

AuraRenderComponent::AuraRenderComponent(GameObject* owner)
	: ComponentBase(owner)
{
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
	//manager.ChangeBlendState(effectLayer ? KdBlendState::EffectAdd : KdBlendState::Add);
	manager.ChangeBlendState(KdBlendState::Alpha);
	manager.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
	manager.ChangeRasterizerState(KdRasterizerState::CullBack);

	if (noiseTex_) { shader.SetAuraNoiseTexture(*noiseTex_); }

	const bool isFlame = (config_.kind == KdAuraKind::Flame);

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
		style.Rise = isFlame ? config_.rise * (1.0f + 0.5f * i) : 0.0f;	// リム型は持ち上げない
		style.BaseY = baseY;
		style.Height = config_.height;
		style.Softness = config_.softness;

		// 描画後に自動で初期値へ戻るので、層ごとに設定し直す
		shader.SetAuraStyle(style);
		shader.DrawModel(*model, world);
	}

	manager.UndoRasterizerState();
	manager.UndoDepthStencilState();
	manager.UndoBlendState();
	shader.EndAura();
}
