#include "Application/main.h"
#include "EffectParams.h"

ParticleBuffer::EmitParameter DirectionalEmitShape::ToEmitParameter(
	const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const
{
	// 速度分布のみを埋める。Size/Life/ColorはParticleAppearance::ApplyTo()が上書きする
	ParticleBuffer::EmitParameter p;
	p.Position = worldPos;
	p.VelocityMin = baseDir * DirScaleMin - OffsetMin;
	p.VelocityMax = baseDir * DirScaleMax + OffsetMax;
	return p;
}

void ParticleAppearance::ApplyTo(ParticleBuffer::EmitParameter& p) const
{
	p.SizeStartMin = SizeStartMin;
	p.SizeStartMax = SizeStartMax;
	p.SizeEndMin = SizeEndMin;
	p.SizeEndMax = SizeEndMax;
	p.LifeMin = LifeMin;
	p.LifeMax = LifeMax;
	p.ColorStartMin = ColorStartMin;
	p.ColorStartMax = ColorStartMax;
	p.ColorMin = ColorMin;
	p.ColorMax = ColorMax;
}

ParticleBuffer::EmitParameter GPUParticleLayer::ToEmitParameter(
	const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const
{
	// Shape由来(速度)のEmitParameterに、Appearance(見た目)とこのLayer固有の
	// ビルボード設定を合成する
	ParticleBuffer::EmitParameter p = Shape.ToEmitParameter(worldPos, baseDir);
	Appearance.ApplyTo(p);
	p.BillboardMode = BillboardMode;
	p.StretchScale = StretchScale;
	return p;
}

bool GPUParticleParams::IsLooping() const
{
	if (EmitMode == ParticleEmitMode::Continuous) { return true; }
	return EmitInterval > 0.0f;
}