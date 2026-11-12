#pragma once

#include <memory>

#include "WindShellPreset.h"
#include "Framework/Shader/ShellShader/WindShellShader.h"

class KdTexture;

// 風シェル1個ぶんの状態(経過時間・消去・変換)。GPU資源は持たない
class WindShellInstance
{
public:

	// seedは模様のずらし用(キーのハッシュ等)。Playのたびにフェードインから始まる
	void Play(const WindShellPreset& preset, float seed, const Math::Vector3& pos, const Math::Vector3& axis);

	// progressは0〜1にクランプされる
	void SetTransform(const Math::Vector3& pos, const Math::Vector3& axis, float progress);

	// 新規の追従を止めて、消去(ディゾルブ)へ移す
	void Stop();

	void Update(float deltaTime);

	bool IsStopping() const { return stopping_; }
	bool IsFinished() const { return stopping_ && lifeRate_ <= 0.0f; }
	float GetRemainingLife() const;

	// 墨(Alpha)→芯(Add)の順に描画する
	void Draw() const;

private:

	void BuildParams(bool core, WindShellParams& out) const;

	WindShellPreset	preset_;
	std::shared_ptr<KdTexture>	streakTex_;
	std::shared_ptr<KdTexture>	noiseTex_;

	Math::Vector3	pos_ = Math::Vector3::Zero;
	Math::Vector3	axis_ = Math::Vector3::Up;
	float	progress_ = 0.0f;
	float	seed_ = 0.0f;
	float	time_ = 0.0f;
	float	lifeRate_ = 1.0f;	// 1→0。消去中のみ減る
	bool	stopping_ = false;
};
