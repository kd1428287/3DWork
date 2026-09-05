#include "../main.h"
#include "EffectParams.h"

KdGPUParticle::EmitParameter DirectionalEmitShape::ToEmitParameter(
	const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Vector3& baseDir) const
{
	KdGPUParticle::EmitParameter p;
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

bool GPUParticleParams::IsLooping() const
{
	if (EmitMode == KdParticleEmitMode::Continuous) { return true; }
	return EmitInterval > 0.0f;
}