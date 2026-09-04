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

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// Main/Emberのデフォルト値 = 元のOnWeaponClash()にあったmainParam/emberParamのハードコード値
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
WeaponClashEffectParams::WeaponClashEffectParams()
{
	// Main：勢いよく飛ぶ火花(発生直後は白熱、飛びながらオレンジへ冷える)
	Main.Shape.DirScaleMin = 1.0f;
	Main.Shape.DirScaleMax = 4.0f;
	Main.Shape.OffsetMin = { 1.0f, 0.5f, 1.0f };
	Main.Shape.OffsetMax = { 1.0f, 1.5f, 1.0f };
	Main.Shape.SizeMin = 0.02f;
	Main.Shape.SizeMax = 0.06f;
	Main.Shape.LifeMin = 0.45f;
	Main.Shape.LifeMax = 0.8f;
	Main.Shape.ColorStartMin = { 2.0f, 2.0f, 1.6f, 1.0f };
	Main.Shape.ColorStartMax = { 2.0f, 2.0f, 1.6f, 1.0f };
	Main.Shape.ColorStartMin = { 0.1f, 0.1f, 0.1f, 1.0f };
	Main.Shape.ColorStartMax = { 0.1f, 0.1f, 0.1f, 1.0f };
	Main.Shape.ColorMin = { 1.0f, 0.85f, 0.4f, 1.0f };
	Main.Shape.ColorMax = { 1.0f, 0.5f, 0.15f, 1.0f };
	Main.CountParry = 120;
	Main.CountBlock = 60;

	// Ember：ゆっくり落ちるくすぶり(最初からやや暗めのオレンジ〜赤)
	Ember.Shape.DirScaleMin = 0.3f;
	Ember.Shape.DirScaleMax = 1.0f;
	Ember.Shape.OffsetMin = { 0.3f, 0.1f, 0.3f };
	Ember.Shape.OffsetMax = { 0.3f, 0.3f, 0.3f };
	Ember.Shape.SizeMin = 0.02f;
	Ember.Shape.SizeMax = 0.05f;
	Ember.Shape.LifeMin = 0.3f;
	Ember.Shape.LifeMax = 0.6f;
	Ember.Shape.ColorStartMin = { 1.2f, 0.7f, 0.3f, 1.0f };
	Ember.Shape.ColorStartMax = { 1.2f, 0.7f, 0.3f, 1.0f };
	Ember.Shape.ColorMin = { 1.0f, 0.5f, 0.1f, 1.0f };
	Ember.Shape.ColorMax = { 0.6f, 0.2f, 0.05f, 1.0f };
	Ember.CountParry = 75;
	Ember.CountBlock = 50;
}