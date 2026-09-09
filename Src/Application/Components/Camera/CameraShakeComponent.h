#pragma once
#include <cmath>
#include <algorithm>
#include "../Transform/TransformComponent.h"

// ============================================================
// カメラシェイク(画面の揺れ)を計算するだけのコンポーネント。
//
// 自分ではtransform_を一切触らない。位置・向きへの反映は
// CameraComponent::PostUpdate()が、Follow→Collisionで確定した
// 座標・向きに対してGetPositionOffset()/GetRotationOffset()を
// 加算する形で行う(詳細はCameraComponent.h冒頭コメント参照)。
// CameraCollisionComponentの平滑化状態(currentDistance_)に
// シェイクの揺れを混入させないため、必ずCollisionより後に
// 加算専用で使うこと。
//
// --- trauma(外傷値)方式 ---------------------------------------------
// 「揺れの強さ」を直接指定させるのではなく、0〜1の外傷値(trauma)を
// 蓄積・減衰させ、実際の揺れ幅はtrauma^2で計算する(Squirrel Eiserloh
// 氏のGDC講演で知られる方式)。この2乗により、小さいヒットの揺れは
// 控えめに、連続ヒットで一気に大きくなるという自然な非線形性が出る。
// 呼び出し側はAddTrauma()を叩くだけでよく、揺れの波形やノイズの
// 実装details を一切知る必要がない。
//
// --- ヒットストップとの関係 -------------------------------------------
// Resolve()に渡すdeltaTimeは、CameraComponent側で意図的に
// SceneContext::unscaledDeltaTime(TimeScaleSystemによるスケールの
// 影響を受けない生のdeltaTime)を渡す想定。ヒットストップで時間が
// ほぼ止まっている間も、シェイクだけは進行させて衝撃を強調する
// 演出のため。Follow/Collisionはスケール済みdeltaTimeを使うため、
// このクラスだけ扱うdeltaTimeの種類が異なる点に注意。
//
// --- ノイズについて -----------------------------------------------
// SampleNoise()は複数周波数のsin波を重ねただけの簡易実装。
// 「ランダムっぽい滑らかな揺れ」としては十分機能するが、より自然な
// 揺れが欲しくなったらこの関数だけをPerlin/Simplexノイズに差し替える
// 想定(呼び出し側のインターフェースは変わらない)。
// ============================================================
class CameraShakeComponent : public ComponentBase {
public:
	explicit CameraShakeComponent(GameObject* owner) : ComponentBase(owner) {}

	// 呼び出し側はこれだけ知っていればよい。目安: 0.3(小さい被弾)
	// 〜1.0(大技のヒット/大爆発)。複数回呼ぶと加算される(最大1.0にクランプ)。
	void AddTrauma(float amount) {
		trauma_ = std::clamp(trauma_ + amount, 0.0f, 1.0f);
	}

	// 現在の外傷値(デバッグ表示用)。
	float GetTrauma() const { return trauma_; }

	// CameraComponent::PostUpdate()から、Collision解決後に呼ばれる想定。
	// deltaTimeにはunscaledDeltaTime(ヒットストップの影響を受けない値)を
	// 渡すこと。クラス冒頭コメント参照。
	void Resolve(float deltaTime) {
		time_ += deltaTime;
		trauma_ = std::max(0.0f, trauma_ - traumaDecayPerSecond_ * deltaTime);

		const float shakePower = trauma_ * trauma_;

		posOffset_ = Math::Vector3(
			SampleNoise(0, time_) * posAmplitude_.x,
			SampleNoise(1, time_) * posAmplitude_.y,
			SampleNoise(2, time_) * posAmplitude_.z) * shakePower;

		// 【要確認】Quaternionの合成順序(親×子 か 子×親か)はプロジェクトの
		// Math::Quaternionの実装規約次第。CameraComponent側で
		// transform_->GetRotation() * GetRotationOffset() の順に掛けている
		// ため、そちらの規約に合わせて調整すること。
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
	// 位置は奥行き(進行方向)を揺らさないのが一般的(酔いにくい)。
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
	float traumaDecayPerSecond_ = 1.5f; // 仮値。「揺れが収まるまでの体感速度」で調整すること
	float noiseFrequency_ = 12.0f;      // 仮値。大きいほど震え、小さいほどうねる

	Math::Vector3 posAmplitude_{ 0.05f, 0.05f, 0.0f }; // Z(奥行き)は揺らさない
	Math::Vector3 rotAmplitude_{ 0.1f, 0.1f, 0.15f }; // ラジアン

	Math::Vector3    posOffset_{};
	Math::Quaternion rotOffset_ = Math::Quaternion::Identity;
};
