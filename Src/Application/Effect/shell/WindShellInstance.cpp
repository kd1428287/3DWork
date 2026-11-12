#include "Application/main.h"
#include "WindShellInstance.h"

void WindShellInstance::Play(const WindShellPreset& preset, float seed, const Math::Vector3& pos, const Math::Vector3& axis)
{
	preset_ = preset;
	seed_ = seed;
	pos_ = pos;
	axis_ = axis;
	progress_ = 0.0f;
	time_ = 0.0f;
	lifeRate_ = 1.0f;
	stopping_ = false;

	streakTex_ = KdAssets::Instance().m_textures.GetData(preset_.StreakTexturePath);
	noiseTex_ = KdAssets::Instance().m_textures.GetData(preset_.NoiseTexturePath);
}

void WindShellInstance::SetTransform(const Math::Vector3& pos, const Math::Vector3& axis, float progress)
{
	pos_ = pos;
	if (axis.LengthSquared() > 1e-8f) { axis_ = axis; }	// 0ベクトルなら前回の向きを保つ
	progress_ = std::clamp(progress, 0.0f, 1.0f);
}

void WindShellInstance::Stop()
{
	stopping_ = true;
}

void WindShellInstance::Update(float deltaTime)
{
	time_ += deltaTime;

	if (!stopping_) { return; }

	// FadeOutTimeが0以下なら即消去
	const float fade = std::max(preset_.FadeOutTime, 1e-4f);
	lifeRate_ = std::max(lifeRate_ - deltaTime / fade, 0.0f);
}

float WindShellInstance::GetRemainingLife() const
{
	return stopping_ ? lifeRate_ * preset_.FadeOutTime : preset_.FadeOutTime;
}

void WindShellInstance::BuildParams(bool core, WindShellParams& out) const
{
	auto lerpP = [this](float a, float b) { return a + (b - a) * progress_; };

	out.World = MakeWindShellMatrix(pos_, axis_);
	out.Color = core ? preset_.CoreColor : preset_.InkColor;

	out.Length = lerpP(preset_.Length0, preset_.Length1) * (core ? preset_.CoreLengthScale : 1.0f);
	out.Radius = lerpP(preset_.Radius0, preset_.Radius1) * (core ? preset_.CoreRadiusScale : 1.0f);
	out.Taper = preset_.Taper;
	out.Twist = lerpP(preset_.Twist0, preset_.Twist1);

	out.Time = time_;
	out.SpinSpeed = lerpP(preset_.SpinSpeed0, preset_.SpinSpeed1) * (core ? preset_.CoreSpinScale : 1.0f);
	out.NoiseAmp = preset_.NoiseAmp;
	out.NoiseFreq = preset_.NoiseFreq;

	out.StreakCount = std::round(preset_.StreakCount);	// 継ぎ目が出ないよう整数にする
	out.StreakStretch = preset_.StreakStretch;
	out.FlowSpeed = preset_.FlowSpeed;
	out.Threshold = preset_.Threshold;

	out.Softness = preset_.Softness;
	out.RimPower = preset_.RimPower;
	out.Scatter = preset_.Scatter;
	out.SpinJitter = preset_.SpinJitter;
	out.NoiseSpeed = preset_.NoiseSpeed;

	const float fadeIn = (preset_.FadeInTime > 0.0f) ? std::clamp(time_ / preset_.FadeInTime, 0.0f, 1.0f) : 1.0f;
	out.Alpha = (core ? preset_.CoreAlpha : preset_.InkAlpha) * fadeIn;
	out.Seed = seed_ + (core ? preset_.CoreSeedOffset : 0.0f);

	out.LifeRate = lifeRate_;
}

void WindShellInstance::Draw() const
{
	if (!streakTex_) { return; }

	KdShaderManager& mgr = KdShaderManager::Instance();
	if (!mgr.m_windShellShader.IsInitialized()) { return; }

	WindShellParams ink;
	BuildParams(false, ink);
	mgr.m_windShellShader.Draw(ink, ParticleBlendMode::Alpha, streakTex_, preset_.Noise, noiseTex_, mgr.GetPerlinNoiseGPU());

	WindShellParams core;
	BuildParams(true, core);
	mgr.m_windShellShader.Draw(core, ParticleBlendMode::Add, streakTex_, preset_.Noise, noiseTex_, mgr.GetPerlinNoiseGPU());
}
