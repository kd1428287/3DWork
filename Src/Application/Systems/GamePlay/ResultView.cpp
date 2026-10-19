#include "Framework/KdFramework.h"

#include "ResultView.h"
#include "ResultController.h"

namespace
{
	constexpr int kAntiAliasing = 3;
	constexpr int kOutlineWidth = 2;

	const Math::Color kInkColor = { 0.04f, 0.04f, 0.05f, 1.0f };
	const Math::Color kPaperColor = { 0.93f, 0.91f, 0.86f, 1.0f };

	// KdFontManagerはShift-JIS前提なので、ワイド文字列から変換して渡す
	std::string ToSjis(const std::wstring& wide)
	{
		const int size = WideCharToMultiByte(932, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (size <= 1) { return {}; }

		std::string sjis(size - 1, '\0');
		WideCharToMultiByte(932, 0, wide.c_str(), -1, sjis.data(), size, nullptr, nullptr);
		return sjis;
	}
}

void ResultView::Build(float playTimeSec)
{
	const int sec = static_cast<int>(playTimeSec);

	// 文字はソースの文字コードに依存しないようエスケープで書く
	wchar_t timeText[48];
	swprintf_s(timeText, L"\u30af\u30ea\u30a2\u6642\u9593  %02d:%02d", sec / 60, sec % 60);	// クリア時間

	const std::wstring texts[kLineCount] =
	{
		L"\u6483\u7834",	// 撃破
		timeText,
		L"クリックでタイトルへ",
	};
	const float scales[kLineCount] = { 2.0f, 1.0f, 0.8f };
	const float ys[kLineCount] = { 60.0f, -40.0f, -150.0f };

	lines_.clear();
	for (int i = 0; i < kLineCount; ++i)
	{
		Line line;
		line.sprite = KdFontManager::Instance().CreateFontTexture(fontNo_, ToSjis(texts[i]), kAntiAliasing);
		line.scale = scales[i];
		line.y = ys[i];
		lines_.push_back(line);
	}
}

void ResultView::Draw(const ResultController& result) const
{
	auto& shader = KdShaderManager::Instance().m_spriteShader;

	for (int i = 0; i < static_cast<int>(lines_.size()); ++i)
	{
		const float alpha = result.GetLineAlpha(i);
		if (alpha <= 0.0f) { continue; }

		const Line& line = lines_[i];
		if (!line.sprite) { continue; }

		Math::Color ink = kInkColor;
		ink.w = alpha;
		Math::Color outline = kPaperColor;
		outline.w = alpha;

		const float lineW = line.sprite->GetTotalWidth() * line.scale;
		const float o = kOutlineWidth * line.scale;

		// pass0=縁 / pass1=本体。縁を描いてから本体を重ねる
		for (int pass = 0; pass < 2; ++pass)
		{
			float x = -lineW * 0.5f;

			for (auto& ch : line.sprite->GetTexList())
			{
				if (!ch->FontTex) { continue; }

				const float w = ch->FontTex->GetInfo().Width * line.scale;
				const float h = ch->FontTex->GetInfo().Height * line.scale;

				if (pass == 0)
				{
					if (auto tex = ch->GetOutlineTex(kOutlineWidth))
					{
						shader.DrawTex(tex.get(), (int)(x - o), (int)(line.y - o), (int)(w + o * 2.0f + 0.5f), (int)(h + o * 2.0f + 0.5f), nullptr, &outline, Math::Vector2(0.0f, 0.0f));
					}
				}
				else
				{
					shader.DrawTex(ch->FontTex.get(), (int)x, (int)line.y, (int)(w + 0.5f), (int)(h + 0.5f), nullptr, &ink, Math::Vector2(0.0f, 0.0f));
				}
				x += w;
			}
		}
	}
}
