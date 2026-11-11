#include "Application/main.h"
#include "ShapeEffectInstance.h"
#include "Framework/Shader/KdShaderManager.h"

void ShapeEffectInstance::Init(std::shared_ptr<IEffectMotion> motion, const ShapeEffectParams& params, ITextureProvider* textureProvider)
{
	motion_ = std::move(motion);
	params_ = params;
	time_ = 0.0f;

	texture_.reset();
	if (textureProvider && !params_.TexturePath.empty())
	{
		texture_ = textureProvider->GetTexture(params_.TexturePath);
	}
}

void ShapeEffectInstance::Draw(const Math::Matrix& base) const
{
	if (!motion_ || !texture_) { return; }

	const EffectPose pose = motion_->Evaluate(time_);

	Math::Vector4 color = params_.Color;
	color.w *= pose.alpha;

	KdShaderManager::Instance().m_shapeEffectShader.Draw(texture_, params_.BlendMode, ComposeWorld(pose, base), color);
}
