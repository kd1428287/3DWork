#pragma once
#include "../../Tags/IModelRenderSource.h"
#include "../../Tags/IRenderable.h"

// Prefab/JSONから受け取る設定(paramsのキー名はメンバ名と同じ)
// ・kind=Rimでは coreColor/rise/height/softness は使われない
struct AuraConfig
{
	KdAuraKind		kind = KdAuraKind::Flame;			// Rim:リムライト型 / Flame:炎型

	Math::Vector3	color = { 1.5f, 0.35f, 0.05f };		// 縁の色(HDR)
	Math::Vector3	coreColor = { 2.5f, 1.6f, 0.5f };	// 炎の芯の色(HDR)
	float			intensity = 1.0f;					// 全体の強さ(0で非表示)
	float			width = 0.08f;						// 1層目の膨らみ量(ワールド単位)
	float			rise = 0.25f;						// 上向きの面を持ち上げる量(炎の背丈)
	float			height = 2.0f;						// 足元から炎の先端までの高さ
	float			softness = 0.15f;					// 炎の縁のぼかし
	float			fresnelPower = 1.5f;				// 大きいほど輪郭側だけが見える
	float			noiseScale = 0.8f;					// ゆらぎの細かさ
	float			scrollSpeed = 0.6f;					// ゆらぎが上へ流れる速さ
	int				layers = 2;							// 重ねる層の数(1～3)
	std::string		noiseTexturePath = "Asset/Textures/System/WhiteNoise.png";	// 滑らかなノイズ画像を指定する
};

// ============================================================
// 同一GameObjectのモデルを法線方向へ膨らませ、加算で重ねてオーラにする。
// 見た目はkindで切り替える(リムライト型 / 輪郭に沿って立ち上る炎型)。
// 加算の実行順が保証されるエフェクトパス(DrawEffect)で描く。
// ============================================================
class AuraRenderComponent : public ComponentBase, public IRenderable
{
public:
	using Config = AuraConfig;

	explicit AuraRenderComponent(GameObject* owner);

	void Awake() override;
	void Update(float deltaTime) override;

	void SetConfig(const Config& config);

	// 演出のON/OFF用(0で非表示)
	void SetIntensity(float intensity) { config_.intensity = intensity; }

	void DrawEffect() override;

private:
	TransformComponent* transform_ = nullptr;
	IModelRenderSource* modelSource_ = nullptr;

	Config config_;
	std::shared_ptr<KdTexture> noiseTex_;

	float time_ = 0.0f;	// ゲーム時間(ヒットストップ等に従う)
};
