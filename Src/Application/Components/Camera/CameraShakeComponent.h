#pragma once
#include <cmath>
#include <algorithm>
#include "../Transform/TransformComponent.h"

// カメラシェイクを計算だけする
// --- ノイズについて -----------------------------------------------
// SampleNoise()は複数周波数のsin波を重ねただけの簡易実装。
// 「ランダムっぽい滑らかな揺れ」としては十分機能するが、より自然な
// 揺れが欲しくなったらこの関数だけをPerlin/Simplexノイズに差し替える
// 想定(呼び出し側のインターフェースは変わらない)。
class CameraShakeComponent : public ComponentBase {
public:
	explicit CameraShakeComponent(GameObject* owner) : ComponentBase(owner) {}

	// 目安: 0.3(小さい被弾) 〜1.0(大技のヒット/大爆発)
	void AddTrauma(float amount) {
		trauma_ = std::clamp(trauma_ + amount, 0.0f, 1.0f);
	}

	// 現在の外傷値
	float GetTrauma() const { return trauma_; }

	void Resolve(float deltaTime) {
		time_ += deltaTime;
		trauma_ = std::max(0.0f, trauma_ - traumaDecayPerSecond_ * deltaTime);

		const float shakePower = trauma_ * trauma_;

		posOffset_ = Math::Vector3(
			SampleNoise(0, time_) * posAmplitude_.x,
			SampleNoise(1, time_) * posAmplitude_.y,
			SampleNoise(2, time_) * posAmplitude_.z) * shakePower;

		rotOffset_ = Math::Quaternion::CreateFromYawPitchRoll(
			SampleNoise(3, time_) * rotAmplitude_.x * shakePower,
			SampleNoise(4, time_) * rotAmplitude_.y * shakePower,
			SampleNoise(5, time_) * rotAmplitude_.z * shakePower);
	}

	Math::Vector3    GetPositionOffset() const { return posOffset_; }
	Math::Quaternion GetRotationOffset() const { return rotOffset_; }

	// 外傷値が減衰しきるまでの体感速度。大きいほどすぐ揺れが収まる。
	void SetTraumaDecay(float perSecond) { traumaDecayPerSecond_ = perSecond; }

	// 揺れの細かさ。大きいほど震え、小さいほどゆったりうねる。
	void SetNoiseFrequency(float frequency) { noiseFrequency_ = frequency; }

	// 位置・回転それぞれの最大振幅(trauma=1.0時の値)。
	void SetPositionAmplitude(const Math::Vector3& amplitude) { posAmplitude_ = amplitude; }
	void SetRotationAmplitude(const Math::Vector3& amplitude) { rotAmplitude_ = amplitude; } // ラジアン

private:
	// 軸ごとに違う位相のノイズが必要なため、axisSeedで位相をずらした
	// 2つのsin波を重ねる。本格的なPerlin/Simplexノイズへの差し替えは
	// この関数の中身だけで完結する。
	float SampleNoise(int axisSeed, float t) const {
		const float freq = noiseFrequency_;
		return std::sin(t * freq * 1.0f + axisSeed * 37.1f) * 0.6f
			 + std::sin(t * freq * 2.7f + axisSeed * 91.7f) * 0.4f;
	}

	float time_ = 0.0f;
	float trauma_ = 0.0f;
	float traumaDecayPerSecond_ = 1.5f; 
	float noiseFrequency_ = 12.0f;    

	Math::Vector3 posAmplitude_{ 0.05f, 0.05f, 0.0f }; 
	Math::Vector3 rotAmplitude_{ 0.1f, 0.1f, 0.15f }; 

	Math::Vector3    posOffset_{};
	Math::Quaternion rotOffset_ = Math::Quaternion::Identity;
};
