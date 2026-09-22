#include "Application/main.h"
#include "EffectParams.h"

ParticleBuffer::EmitParameter DirectionalEmitShape::ToEmitParameter(
	const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const
{
	ParticleBuffer::EmitParameter p;
	p.Position = worldPos;
	p.VelocityMin = baseDir * DirScaleMin - OffsetMin;
	p.VelocityMax = baseDir * DirScaleMax + OffsetMax;
	p.SizeMin = SizeMin;
	p.SizeMax = SizeMax;
	p.LifeMin = LifeMin;
	p.LifeMax = LifeMax;
	p.ColorStartMin = ColorStartMin;
	p.ColorStartMax = ColorStartMax;
	p.ColorMin = ColorMin;
	p.ColorMax = ColorMax;
	return p;
}

ParticleBuffer::EmitParameter GPUParticleLayer::ToEmitParameter(
	const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const
{
	// Shape由来(速度・サイズ・寿命・色)のEmitParameterに、
	// このLayer固有のビルボード設定を合成する
	ParticleBuffer::EmitParameter p = Shape.ToEmitParameter(worldPos, baseDir);
	p.BillboardMode = BillboardMode;
	p.StretchScale = StretchScale;
	return p;
}

bool GPUParticleParams::IsLooping() const
{
	if (EmitMode == ParticleEmitMode::Continuous) { return true; }
	return EmitInterval > 0.0f;
}