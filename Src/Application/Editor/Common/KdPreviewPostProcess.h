#pragma once

//============================================================
// KdPreviewPostProcess
//	エディタのプレビューウィンドウ(EffectEditor/MapEditor等。可変サイズ・複数同時存在しうる)
//	向けの軽量なポストプロセス。
//
//	・現時点ではカラーグレードのみを適用する(Blur/DoF/Bloomは持たない。将来必要になれば
//	  同じ方針(KdPostProcessShaderの処理を引数化して呼ぶ)で段階的に追加できる)
//	・VS/PS/定数バッファは本編用シングルトンであるKdPostProcessShaderのものを
//	  そのまま使い回す(KdPostProcessShader::ApplyColorGradeOnly()経由)。
//	  このクラス自身が持つのは「プレビューサイズに合わせた出力テクスチャ」と
//	  「カラーグレードPSが要求するマスク入力(t1)用の黒ダミーテクスチャ」だけ。
//	・本編のKdPostProcessShaderが持つ固定サイズのRTPack群(m_postEffectRTPack等)には
//	  一切触れないため、本編のポストプロセスパイプラインへの影響は無い。
//
//	使い方(想定)：
//		EffectEditor/MapEditorの RenderPreviewViewport() 内で、
//		プレビュー用オフスクリーンへの描画が全て終わった直後に
//			m_previewPostProcess.Apply(m_previewViewport.Color);
//		を呼び、以降 ImGui::Image() で表示するテクスチャを
//			m_previewViewport.Color  →  m_previewPostProcess.GetResultTexture()
//		へ差し替える。
//============================================================
class KdPreviewPostProcess
{
public:

	// srcColorへカラーグレードを適用し、内部の出力テクスチャへ書き込む。
	// 出力テクスチャはsrcColorのサイズに合わせて内部で自動的に作り直される(Resize不要)。
	// 結果はGetResultTexture()で取得する
	void Apply(const std::shared_ptr<KdTexture>& srcColor);

	// カラーグレード適用後の結果テクスチャ(表示にはこちらを使う)
	// 一度もApply()していない場合はnullptrを返す
	const std::shared_ptr<KdTexture>& GetResultTexture() const { return m_resultTex; }

private:

	// 出力テクスチャをwidth x heightで(必要なら)作り直す
	void ResizeResultTex(int width, int height);

	// カラーグレードPSが要求するマスク入力(t1)用の1x1黒ダミーテクスチャを(未生成なら)作る
	// 黒(0) = 「マスク無し・グレーディングを通常通り適用」を意味する(本編の規約と同じ)
	void EnsureDummyMaskTex();

	std::shared_ptr<KdTexture>	m_resultTex;		// グレーディング後の出力
	std::shared_ptr<KdTexture>	m_dummyMaskTex;		// カラーグレードPS用の黒ダミー(1x1、使い回し)

	int		m_width = 0;
	int		m_height = 0;
};
