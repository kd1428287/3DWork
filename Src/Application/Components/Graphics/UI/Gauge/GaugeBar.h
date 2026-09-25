#pragma once
#include "GaugeBarRenderer.h"

//===================================================
//
// ゲージ1本分の「種類ごとの既定の見た目/挙動(プリセット)」と、
// 表示状態(スムージング・フェード・色の変化)を持つ共通クラス
// ・種類はGaugeBarTypeで選ぶ。描画側はプリセットの中身(GaugeFillMode等)だけを見る
// ・使う側の都合で見た目を変えたい場合はLoad前にPreset()を上書きする
//
//===================================================
enum class GaugeBarType
{
	Health,
	Posture,
};

struct GaugeBarPreset
{
	explicit GaugeBarPreset(const GaugeBarStyle& s) : style(s) {}

	GaugeBarStyle style;
	GaugeFillMode fillMode = GaugeFillMode::LeftToRight;
	bool capsule = false;
	Math::Color fillColor = kWhiteColor;

	// 0付近で非表示にする(フェードイン/アウト)
	bool hideWhenEmpty = false;
	// 表示割合が目標へ追従する速さ(1/秒)。0で即時
	float smoothSpeed = 0.0f;
	// 透明度の変化の速さ(1/秒)
	float fadeSpeed = 6.0f;

	// 割合に応じた色の段階(fillColor→midColor→highColor。midAtで中間色になる)
	bool colorRamp = false;
	Math::Color midColor = kWhiteColor;
	Math::Color highColor = kWhiteColor;
	float midAt = 0.6f;

	// 表示割合がpulseThreshold以上で色を白へ脈動させる(1超で無効)
	float pulseThreshold = 2.0f;
	float pulseSpeed = 14.0f;	// 位相の進み(rad/秒)
	float pulseAmount = 0.5f;	// 白へ寄せる最大の割合

	// 割合が増えた瞬間に白く光らせる(0で無効)。flashDecayは減衰の速さ(1/秒)
	float flashAmount = 0.0f;
	float flashDecay = 6.0f;
};

// 種類→プリセットの対応表。プリセットを増やすのはここだけ
inline GaugeBarPreset MakeGaugeBarPreset(GaugeBarType type)
{
	switch (type)
	{
	case GaugeBarType::Posture:
	{
		GaugeBarPreset p{ GaugeBarStyle{ "Asset/Textures/UI/posture_back.png", "", 2 } };
		p.fillMode = GaugeFillMode::CenterOut;
		p.fillColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		p.hideWhenEmpty = true;
		p.smoothSpeed = 10.0f;
		p.colorRamp = true;
		p.midColor = { 1.0f, 0.6f, 0.15f, 1.0f };
		p.highColor = { 0.9f, 0.1f, 0.1f, 1.0f };
		p.pulseThreshold = 0.85f;
		p.flashAmount = 0.7f;
		
		return p;
	}
	case GaugeBarType::Health:
	default:
	{
		GaugeBarPreset p{ GaugeBarStyle{ "Asset/Textures/UI/hp_back.png", "Asset/Textures/UI/hp_fill.png", 0 } };
		p.capsule = true;
		p.fillColor = { 0.8f, 0.1f, 0.1f, 1.0f };
		return p;
	}
	}
}

class GaugeBar
{
public:
	explicit GaugeBar(GaugeBarType type)
		: preset_(MakeGaugeBarPreset(type)), alpha_(preset_.hideWhenEmpty ? 0.0f : 1.0f) {}

	// Loadより前に上書きすること
	GaugeBarPreset& Preset() { return preset_; }

	void Load()
	{
		skin_.Load(preset_.style);
		skin_.capsule = preset_.capsule;
		skin_.fillMode = preset_.fillMode;
		skin_.fillColor = preset_.fillColor;
	}

	// 毎フレーム呼ぶ。dtにはUIなのでスケールされていない経過時間を渡す
	void Update(float dt, float targetRatio)
	{
		const float target = std::clamp(targetRatio, 0.0f, 1.0f);

		if (!initialized_)
		{
			display_ = target;
			prevTarget_ = target;
			initialized_ = true;
		}
		else
		{
			// 増えた瞬間だけ光らせる(回復などの減少では光らせない)
			if (target > prevTarget_ + 0.001f) { flash_ = 1.0f; }
			prevTarget_ = target;

			if (preset_.smoothSpeed <= 0.0f)
			{
				display_ = target;
			}
			else
			{
				display_ += (target - display_) * (1.0f - std::exp(-preset_.smoothSpeed * dt));
			}
		}

		flash_ = std::max(0.0f, flash_ - preset_.flashDecay * dt);
		pulsePhase_ += preset_.pulseSpeed * dt;

		// 表示割合が縮み切るまでは見せ続け、そこからフェードアウトする
		const bool empty = preset_.hideWhenEmpty && target <= 0.001f && display_ <= 0.001f;
		const float goal = empty ? 0.0f : 1.0f;
		const float step = preset_.fadeSpeed * dt;
		alpha_ += std::clamp(goal - alpha_, -step, step);

		skin_.fillColor = CalcFillColor();
	}

	void Draw(KdSpriteShader& shader, const Math::Vector2& pos, const Math::Vector2& size,
		const Math::Vector2& pivot, const Math::Color& tint = kWhiteColor) const
	{
		if (alpha_ <= 0.0f) { return; }

		Math::Color t = tint;
		t.w *= alpha_;
		GaugeBarRenderer::Draw(shader, pos, size, display_, skin_, t, pivot);
	}

private:
	// 色の段階→脈動→フラッシュの順に重ねる(見た目に合わせて表示割合で判定する)
	Math::Color CalcFillColor() const
	{
		Math::Color c = preset_.fillColor;

		if (preset_.colorRamp)
		{
			c = (display_ < preset_.midAt)
				? Math::Color::Lerp(preset_.fillColor, preset_.midColor, display_ / preset_.midAt)
				: Math::Color::Lerp(preset_.midColor, preset_.highColor,
					(display_ - preset_.midAt) / std::max(1.0f - preset_.midAt, 0.001f));
		}

		if (display_ >= preset_.pulseThreshold)
		{
			const float k = 0.5f + 0.5f * std::sin(pulsePhase_);
			c = Math::Color::Lerp(c, kWhiteColor, k * preset_.pulseAmount);
		}

		if (flash_ > 0.0f)
		{
			c = Math::Color::Lerp(c, kWhiteColor, flash_ * preset_.flashAmount);
		}
		return c;
	}

	GaugeBarPreset preset_;
	GaugeBarSkin skin_;
	float display_ = 0.0f;
	float prevTarget_ = 0.0f;
	float alpha_ = 1.0f;
	float flash_ = 0.0f;
	float pulsePhase_ = 0.0f;
	bool initialized_ = false;
};