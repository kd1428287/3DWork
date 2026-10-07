#include "Framework/KdFramework.h"

#include "TextComponent.h"
#include "Application/Components/Graphics/UI/UITransformComponent.h"

void TextComponent::Awake()
{
	ui_ = GetOwner()->GetComponent<UITransformComponent>();
}

void TextComponent::SetText(const std::string& text)
{
	if (m_text == text) return;
	m_text = text;
	m_dirty = true;
}

void TextComponent::SetFontNo(int fontNo)
{
	// KdFontManagerの登録数(0～9)に収める
	fontNo = std::clamp(fontNo, 0, 9);
	if (m_fontNo == fontNo) return;
	m_fontNo = fontNo;
	m_dirty = true;
}

void TextComponent::SetAntiAliasing(int level)
{
	level = std::clamp(level, 0, 3);
	if (m_antiAliasing == level) return;
	m_antiAliasing = level;
	m_dirty = true;
}

void TextComponent::Rebuild()
{
	m_dirty = false;
	m_lines.clear();
	m_lineHeight = 0;

	// '\n'で行に分割(Shift-JISの2バイト目に0x0Aは現れないので安全)
	size_t start = 0;
	while (true)
	{
		size_t end = m_text.find('\n', start);
		std::string line = m_text.substr(start, end == std::string::npos ? std::string::npos : end - start);

		if (!line.empty() && line.back() == '\r') line.pop_back();

		auto sprite = KdFontManager::Instance().CreateFontTexture(m_fontNo, line, m_antiAliasing);

		// 行の高さは全文字テクスチャの最大値
		for (auto& ch : sprite->GetTexList())
		{
			if (ch->FontTex) m_lineHeight = std::max(m_lineHeight, (float)ch->FontTex->GetInfo().Height);
		}
		m_lines.push_back(sprite);

		if (end == std::string::npos) break;
		start = end + 1;
	}
}

void TextComponent::DrawSprite()
{
	// UITransformがAwake後に追加された場合に備えて、ここでも探す
	if (!ui_) ui_ = GetOwner()->GetComponent<UITransformComponent>();
	if (!ui_)
	{
		if (!m_warned)
		{
			m_warned = true;
			OutputDebugStringA("TextComponent: UITransformComponent is required\n");
		}
		return;
	}

	if (m_dirty) Rebuild();
	if (m_lines.empty()) return;

	Math::Vector2 base;
	if (!ui_->Resolve(base)) return;

	const Math::Vector2 scale = ui_->GetScale();
	const Math::Vector2 pivot = ui_->GetPivot();

	// 文字ブロックの大きさ(拡大率適用後)
	const float lineH = m_lineHeight * scale.y;
	float blockW = 0.0f;
	for (auto& line : m_lines) blockW = std::max(blockW, line->GetTotalWidth() * scale.x);
	const float blockH = lineH * (float)m_lines.size();

	// pivotがブロックのどこかを基準点に合わせる。Y上向きなので上端はbase.y + blockH*(1-pivot.y)
	const float left = base.x - blockW * pivot.x;
	float y = base.y + blockH * (1.0f - pivot.y) - lineH;

	auto& shader = KdShaderManager::Instance().m_spriteShader;

	for (auto& line : m_lines)
	{
		const float lineW = line->GetTotalWidth() * scale.x;

		float x = left;
		if (m_align == Align::Center)		x += (blockW - lineW) * 0.5f;
		else if (m_align == Align::Right)	x += blockW - lineW;

		for (auto& ch : line->GetTexList())
		{
			if (!ch->FontTex) continue;

			const float w = ch->FontTex->GetInfo().Width * scale.x;
			const float h = ch->FontTex->GetInfo().Height * scale.y;

			// pivot(0,0)で左下基準になる
			shader.DrawTex(ch->FontTex.get(), (int)x, (int)y, (int)(w + 0.5f), (int)(h + 0.5f), nullptr, &m_color, Math::Vector2(0.0f, 0.0f));
			x += w;
		}
		y -= lineH;
	}
}