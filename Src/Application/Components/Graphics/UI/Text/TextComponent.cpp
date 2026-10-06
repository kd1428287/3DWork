#include "Framework/KdFramework.h"

#include "TextComponent.h"

namespace
{
	Math::Matrix s_view = Math::Matrix::Identity;
	Math::Matrix s_proj = Math::Matrix::Identity;

	// ワールド座標を2D描画用の座標(中心原点・Y上向き)へ変換。カメラの後ろならfalse
	bool ProjectToScreen(const Math::Vector3& world, Math::Vector2& out)
	{
		UINT num = 1;
		D3D11_VIEWPORT vp;
		KdDirect3D::Instance().WorkDevContext()->RSGetViewports(&num, &vp);

		Math::Vector4 clip = Math::Vector4::Transform(Math::Vector4(world.x, world.y, world.z, 1.0f), s_view * s_proj);
		if (clip.w <= 0.0f) return false;

		out.x = clip.x / clip.w * vp.Width * 0.5f;
		out.y = clip.y / clip.w * vp.Height * 0.5f;
		return true;
	}
}

void TextComponent::SetCamera(const Math::Matrix& view, const Math::Matrix& proj)
{
	s_view = view;
	s_proj = proj;
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
	if (m_dirty) Rebuild();

	// 基準座標。Worldの場合はTransform位置を投影し、ピクセルオフセットを足す
	Math::Vector2 base = m_screenPos;
	if (m_space == Space::World)
	{
		if (!transform_) return;

		Math::Vector2 sp;
		if (!ProjectToScreen(transform_->GetPosition() + m_worldOffset, sp)) return;
		base += sp;
	}

	auto& shader = KdShaderManager::Instance().m_spriteShader;

	// Y軸は上向きなので、1行目の上端を基準に行ごとにYを減らす
	float y = base.y - m_lineHeight;
	for (auto& line : m_lines)
	{
		float x = base.x;
		if (m_align == Align::Center)		x -= line->GetTotalWidth() * 0.5f;
		else if (m_align == Align::Right)	x -= (float)line->GetTotalWidth();

		for (auto& ch : line->GetTexList())
		{
			if (!ch->FontTex) continue;

			// pivot(0,0)で左下基準になる
			shader.DrawTex(ch->FontTex.get(), (int)x, (int)y, nullptr, &m_color, Math::Vector2(0.0f, 0.0f));
			x += (float)ch->FontTex->GetInfo().Width;
		}
		y -= m_lineHeight;
	}
}
