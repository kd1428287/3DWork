#include "Application/main.h"
#include "ShapeEffectInstance.h"
#include "Framework/Shader/KdShaderManager.h"

namespace
{
	// ビルボード時のワールド行列。位置は基準点ローカルのpose.posを基準点でワールド化した点、
	// 向きはカメラから決める(基準点・poseの回転は使わない。Fullのみpose.rotDeg.zをロールとして使う)
	Math::Matrix ComposeBillboardWorld(const EffectPose& pose, const Math::Matrix& base, ShapeBillboardMode mode)
	{
		const KdShaderManager::cbCamera& camera = KdShaderManager::Instance().GetCameraCB();

		const Math::Vector3 center = Math::Vector3::Transform(pose.pos, base);

		Math::Matrix orientation;
		bool useYAxis = (mode == ShapeBillboardMode::YAxis);

		if (useYAxis)
		{
			// カメラの真上/真下では水平方向が定まらないため、Fullへ切り替える
			Math::Vector3 forward = center - camera.CamPos;
			forward.y = 0.0f;
			if (forward.LengthSquared() > 1e-6f)
			{
				forward.Normalize();
				orientation = Math::Matrix(Math::Vector3::Up.Cross(forward), Math::Vector3::Up, forward);
			}
			else { useYAxis = false; }
		}

		if (!useYAxis)
		{
			orientation = camera.mView.Invert();
			orientation.Translation(Math::Vector3::Zero);
		}

		const float rollRad = useYAxis ? 0.0f : DirectX::XMConvertToRadians(pose.rotDeg.z);

		return Math::Matrix::CreateScale(pose.scale.x, pose.scale.y, 1.0f)
			* Math::Matrix::CreateRotationZ(rollRad)
			* orientation
			* Math::Matrix::CreateTranslation(center);
	}
}

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

	ShapeEffectDrawInfo info;
	info.Texture = texture_;
	info.BlendMode = params_.BlendMode;
	info.World = (params_.Billboard == ShapeBillboardMode::None)
		? ComposeWorld(pose, base)
		: ComposeBillboardWorld(pose, base, params_.Billboard);
	info.Color = params_.Color;
	info.Color.w *= pose.alpha;
	info.DepthOffset = params_.DepthOffset;
	info.DepthTest = params_.DepthTest;

	KdShaderManager::Instance().m_shapeEffectShader.Draw(info);
}
