#pragma once

#include "../GPUParticle/KdGPUParticle.h"	// KdParticleBlendMode 等、同じ低レイヤー内で完結させる為

//====================================================================
//
// トレイル(斬撃の軌跡)描画用シェーダー(VS/PS/InputLayout)を
// 全SlashTrailInstance/SlashTrailRendererで共有管理するクラス
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 【経緯】
//	以前はSlashTrailRendererがインスタンス(＝斬撃1本)ごとにVS/PS/InputLayoutを
//	生成していた為、攻撃のたびにシェーダーオブジェクトを作り直す無駄があった。
//	KdStandardShader/KdPostProcessShader等と同じく「シェーダーはKdShaderManagerが
//	1個だけ保持して共有し、インスタンス固有なのは動的頂点バッファだけ」という形に
//	統一する為に切り出した。
//
// 【所有・生成タイミング】
//	KdShaderManagerがメンバとして1個だけ保持し、KdShaderManager::Init()相当の
//	タイミングでInit()を1回だけ呼ぶ想定(頂点シェーダー/ピクセルシェーダーの実体は
//	SlashTrailRenderer.cppに元々あったものをそのまま移設しただけで、シェーダーの
//	中身自体は変更していない)。
//
// 【使い方】
//   // 起動時に1回(KdShaderManager::Init()内)
//   m_slashTrailShader.Init();
//
//   // 毎フレーム、SlashTrailRenderer::Draw()の中から
//   //	(他のシェーダー同様、KdShaderManagerはgetterを介さずpublicメンバを直接使うスタイル)
//   KdShaderManager::Instance().m_slashTrailShader.Begin();
//   : DevCon->Draw(...) ;
//   KdShaderManager::Instance().m_slashTrailShader.End();
//====================================================================
class SlashTrailShader
{
public:

	SlashTrailShader() {}
	~SlashTrailShader() { Release(); }

	// コピー禁止(D3D11の生ポインタ資源を持つ為。KdStandardShader等と同じ理由)
	SlashTrailShader(const SlashTrailShader&) = delete;
	SlashTrailShader& operator=(const SlashTrailShader&) = delete;

	// VS/PS/InputLayoutの生成。KdShaderManager初期化時に1回だけ呼ばれる想定
	bool Init();

	void Release();

	// VS/PS/InputLayoutのバインドのみ行う。
	//	頂点バッファのセット・ドローコールは呼び出し側(SlashTrailRenderer)の責務のまま
	void Begin();

	// 現状はBegin()側でカスタムのRS/DSS等を直接いじっていない為、対称性のためだけに
	// 用意している空実装。将来Begin側で個別ステートを触るようになった場合はここで戻す
	void End();

	bool IsInitialized() const { return m_initialized; }

private:

	bool	m_initialized = false;

	ID3D11VertexShader*	m_VS = nullptr;			// SlashTrail_VS.hlsl(新規)
	ID3D11PixelShader*	m_PS = nullptr;			// KdGPUParticle_PS.hlslを流用
	ID3D11InputLayout*	m_inputLayout = nullptr;
};
