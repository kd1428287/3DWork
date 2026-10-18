#pragma once

#include <memory>
#include <vector>

class KdFontSprite;
class ResultController;

// リザルト文字の描画。KdFontのテクスチャを画面中央基準で並べる
class ResultView
{
public:
	static constexpr int kLineCount = 3;

	// 使うフォント番号(KdFontManager::AddFontの登録順)
	void SetFontNo(int fontNo) { fontNo_ = fontNo; }

	// 文字列を作り直す。撃破時に1回呼ぶ
	void Build(float playTimeSec);

	// UI描画の最後に呼ぶ。各行の不透明度はResultControllerから取る
	void Draw(const ResultController& result) const;

private:
	struct Line
	{
		std::shared_ptr<KdFontSprite> sprite;
		float scale = 1.0f;
		float y = 0.0f;
	};

	int fontNo_ = 0;
	std::vector<Line> lines_;
};
