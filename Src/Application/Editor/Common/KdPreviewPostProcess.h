#pragma once

//============================================================
// KdPreviewPostProcess
//	エディタのプレビューウィンドウ(EffectEditor/MapEditor等。可変サイズ・複数同時存在しうる)
//	向けの軽量なポストプロセス。
//
//	・カラーグレードに加え、簡易的なBloom(Light Bloom)を適用できる。
//	  Bloomは「srcBright(Bright/加算描画専用のオフスクリーン)に描かれた内容をぼかして
//	  srcColorへ加算合成する」だけの単純な処理で、本編のLightBloomProcess()と違い
//	  シーン全体からの輝度抽出(閾値によるBrightPS抽出)は行わない。エフェクトプレビューは
//	  元々パーティクルしか描いていないため、Bright方面の直接描画だけで十分な為
//	・VS/PS/定数バッファは本編用シングルトンであるKdPostProcessShaderのものを
//	  そのまま使い回す(KdPostProcessShader::ApplyColorGradeOnly()/GenerateBlurTexture()経由)。
//	  このクラス自身が持つのは「プレビューサイズに合わせた出力・Bloom用テクスチャ」と
//	  「カラーグレードPSが要求するマスク入力(t1)用の黒ダミーテクスチャ」だけ。
//	・本編のKdPostProcessShaderが持つ固定サイズのRTPack群(m_postEffectRTPack等)には
//	  一切触れないため、本編のポストプロセスパイプラインへの影響は無い。
//
//	使い方(想定)：
//		EffectEditor/MapEditorの RenderPreviewViewport() 内で、
//		プレビュー用オフスクリーンへの描画が全て終わった直後に
//			m_previewPostProcess.Apply(m_previewViewport.Color, m_previewViewport.Bright);
//		を呼び(Bloom不要ならBrightはnullptrのまま省略可)、以降 ImGui::Image() で表示する
//		テクスチャを
//			m_previewViewport.Color  →  m_previewPostProcess.GetResultTexture()
//		へ差し替える。
//============================================================
class KdPreviewPostProcess
{
public:

	// srcColorへ(渡されていればBloom→)カラーグレードを適用し、内部の出力テクスチャへ書き込む。
	// 出力テクスチャはsrcColorのサイズに合わせて内部で自動的に作り直される(Resize不要)。
	// srcBright … Bright(加算合成)専用に描画されたオフスクリーン。省略時はBloomを適用しない
	// 結果はGetResultTexture()で取得する
	void Apply(const std::shared_ptr<KdTexture>& srcColor, const std::shared_ptr<KdTexture>& srcBright = nullptr);

	// カラーグレード適用後の結果テクスチャ(表示にはこちらを使う)
	// 一度もApply()していない場合はnullptrを返す
	const std::shared_ptr<KdTexture>& GetResultTexture() const { return m_resultTex; }

private:

	// 出力テクスチャをwidth x heightで(必要なら)作り直す
	void ResizeResultTex(int width, int height);

	// カラーグレードPSが要求するマスク入力(t1)用の1x1黒ダミーテクスチャを(未生成なら)作る
	// 黒(0) = 「マスク無し・グレーディングを通常通り適用」を意味する(本編の規約と同じ)
	void EnsureDummyMaskTex();

	// srcBrightをぼかしながら段階的に合成し、srcColorへ加算合成する(srcColor自体を書き換える)
	void ApplyBloom(const std::shared_ptr<KdTexture>& srcColor, const std::shared_ptr<KdTexture>& srcBright);

	// Bloom用のぼかしテクスチャをwidth x heightで(必要なら)作り直す
	void ResizeBloomTex(int width, int height);

	std::shared_ptr<KdTexture>	m_resultTex;		// グレーディング後の出力
	std::shared_ptr<KdTexture>	m_dummyMaskTex;		// カラーグレードPS用の黒ダミー(1x1、使い回し)

	int		m_width = 0;
	int		m_height = 0;

	// Bloom用ぼかしテクスチャ群。本編(4段・kBlurSamplingRadius=8)より簡略化した軽量版
	// (プレビュー用途なので画質より生成コストを優先。全段階srcColorと同じ解像度で処理する)
	static const int	kBloomNum = 3;
	static const int	kBloomBlurRadius = 6;

	std::shared_ptr<KdTexture>	m_bloomTex[kBloomNum];
	int		m_bloomWidth = 0;
	int		m_bloomHeight = 0;
};