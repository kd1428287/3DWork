#pragma once

class KdFontSprite;

// 文字列描画コンポーネント
// Screen: 画面座標に表示 / World: Transformの位置に追従して表示
class TextComponent : public ComponentBase, public IRenderable {
public:
	enum class Align { Left, Center, Right };
	enum class Space { Screen, World };

	explicit TextComponent(GameObject* owner) :ComponentBase(owner) {}

	void Awake() override {
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	void DrawSprite() override;

	// 文字列(Shift-JIS)。'\n'で改行できる
	void SetText(const std::string& text);
	// KdFontManager::AddFontで登録済みのフォント番号(0～9)
	void SetFontNo(int fontNo);
	// アンチエイリアス 0:なし 1:2x 2:4x 3:8x
	void SetAntiAliasing(int level);

	void SetSpace(Space space)						{ m_space = space; }
	void SetAlign(Align align)						{ m_align = align; }
	void SetColor(const Math::Color& color)			{ m_color = color; }
	// Screen: 画面座標(中心原点・Y上向き) / World: 投影後のピクセルオフセット
	void SetScreenPos(const Math::Vector2& pos)		{ m_screenPos = pos; }
	// World時にTransform位置へ足すオフセット(頭上に出す分など)
	void SetWorldOffset(const Math::Vector3& offset){ m_worldOffset = offset; }

	const std::string& GetText() const				{ return m_text; }

	// World表示の投影に使うカメラ行列。カメラ更新時に毎フレーム設定する
	static void SetCamera(const Math::Matrix& view, const Math::Matrix& proj);

private:
	// 行ごとのフォントスプライトを作り直す
	void Rebuild();

	TransformComponent*	transform_		= nullptr;

	std::string			m_text;
	int					m_fontNo		= 0;
	int					m_antiAliasing	= 3;

	Space				m_space			= Space::Screen;
	Align				m_align			= Align::Left;
	Math::Color			m_color			= { 1, 1, 1, 1 };
	Math::Vector2		m_screenPos		= { 0, 0 };
	Math::Vector3		m_worldOffset	= { 0, 0, 0 };

	std::vector<std::shared_ptr<KdFontSprite>>	m_lines;
	float				m_lineHeight	= 0;
	bool				m_dirty			= true;
};
