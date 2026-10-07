#pragma once

class KdFontSprite;
class UITransformComponent;

// 文字列描画コンポーネント
// 位置・pivot・拡大率は同じオブジェクトのUITransformComponentから取得する(必須)
class TextComponent : public ComponentBase, public IRenderable {
public:
	// 複数行のときの行ごとの揃え。文字ブロック全体の位置はUITransformのpivotで決まる
	enum class Align { Left, Center, Right };

	explicit TextComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override;
	void DrawSprite() override;

	// 文字列(Shift-JIS)。'\n'で改行できる
	void SetText(const std::string& text);
	// KdFontManager::AddFontで登録済みのフォント番号(0～9)
	void SetFontNo(int fontNo);
	// アンチエイリアス 0:なし 1:2x 2:4x 3:8x
	void SetAntiAliasing(int level);

	void SetAlign(Align align) { m_align = align; }
	void SetColor(const Math::Color& color) { m_color = color; }

	const std::string& GetText() const { return m_text; }

private:
	// 行ごとのフォントスプライトを作り直す
	void Rebuild();

	UITransformComponent* ui_ = nullptr;

	std::string			m_text;
	int					m_fontNo = 0;
	int					m_antiAliasing = 3;

	Align				m_align = Align::Left;
	Math::Color			m_color = { 1, 1, 1, 1 };

	std::vector<std::shared_ptr<KdFontSprite>>	m_lines;
	float				m_lineHeight = 0;
	bool				m_dirty = true;
	bool				m_warned = false;
};