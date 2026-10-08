#pragma once

class UITransformComponent;

// 画像(テクスチャが無ければ単色の矩形)を表示するUIコンポーネント
// 位置・pivot・拡大率・回転は同じオブジェクトのUITransformComponentから取得する(必須)。
// UITransformのsizeが0のときは、テクスチャの大きさで表示する
class UIImageComponent : public ComponentBase, public IRenderable {
public:
	explicit UIImageComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override;
	void DrawSprite() override;

	// テクスチャのパス(Shift-JIS)。空文字なら単色の矩形になる
	void SetTexture(const std::string& path);
	void SetColor(const Math::Color& color) { m_color = color; }

private:
	UITransformComponent* ui_ = nullptr;

	std::shared_ptr<KdTexture>	m_texture;
	Math::Color					m_color = { 1, 1, 1, 1 };
	bool						m_warned = false;
};
