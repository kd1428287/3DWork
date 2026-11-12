#pragma once

#include "../GPUParticle/ParticleBuffer.h"	// ParticleBlendMode / ParticleNoiseParams を流用する為
#include "PerlinNoiseGPU.h"

//====================================================================
//
// 風をまとうエフェクト(回転体シェル)を描画するシェーダー
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// ・形(カプセル/球・ねじれ・ノイズ)はVS(WindShell_VS.hlsl)で作る。メッシュは共通のグリッド1つだけ
// ・筋の見た目はPS(WindShell_PS.hlsl)で、筋テクスチャのしきい値切り抜きで作る
// ・消去は、トレイルと同じパーティクルノイズ(SlashTrailShader::BindNoise)を使う
// ・KdShaderManagerが1個だけ保持して共有する(SlashTrailShaderと同じ)
//
// 【使い方】
//   KdShaderManager::Init()内で m_windShellShader.Init() を1回呼ぶ。
//   Litパスの中で、墨(Alpha)→芯(Add)の順にDraw()を呼ぶ。
//====================================================================

// HLSL側cbWindShell(inc_WindShell.hlsli)と同じレイアウト(160バイト)
struct WindShellParams
{
	Math::Matrix	World = Math::Matrix::Identity;	// 回転+移動のみ(+Yが軸。スケールは入れない)
	Math::Vector4	Color = { 1.0f, 1.0f, 1.0f, 1.0f };

	float			Length = 0.5f;			// 円筒部の長さ(0で球)
	float			Radius = 0.2f;			// 半径(両端の半球も同じ半径)
	float			Taper = 0.5f;			// 後方のすぼまり(0-1)
	float			Twist = 4.0f;			// 軸方向のねじれ量(ラジアン)

	float			Time = 0.0f;
	float			SpinSpeed = 8.0f;		// 回転速度(ラジアン/秒)
	float			NoiseAmp = 0.3f;		// 頂点ノイズによる半径の揺らぎ
	float			NoiseFreq = 1.5f;

	float			StreakCount = 6.0f;		// 周方向の筋の繰り返し数(整数にする事)
	float			StreakStretch = 1.0f;	// 軸方向のUV倍率(小さいほど長い筋)
	float			FlowSpeed = 1.5f;		// 筋が軸方向へ流れる速さ
	float			Threshold = 0.5f;		// 筋の切り抜きしきい値

	float			Softness = 0.1f;		// 切り抜きの柔らかさ
	float			RimPower = 2.0f;		// 輪郭付近を強調する指数
	float			Alpha = 1.0f;
	float			Seed = 0.0f;			// レイヤーごとの位相ずらし

	float			LifeRate = 1.0f;		// 消去用 1→0
	float			Scatter = 0.0f;		// 筋UVの歪み量
	float			SpinJitter = 0.0f;	// 位置ごとの回転角のゆらぎ(ラジアン)
	float			NoiseSpeed = 0.0f;	// 半径ノイズの時間変化の速さ
};
static_assert(sizeof(WindShellParams) == 160, "cbWindShellのサイズと一致させる事");

// +Yをaxisへ向けた剛体行列(スケールなし)を作る
inline Math::Matrix MakeWindShellMatrix(const Math::Vector3& pos, Math::Vector3 axis)
{
	if (axis.LengthSquared() < 1e-8f) { axis = Math::Vector3::Up; }
	axis.Normalize();

	const Math::Vector3 tmp = (fabsf(axis.y) > 0.99f) ? Math::Vector3::Right : Math::Vector3::Up;
	Math::Vector3 right = tmp.Cross(axis);
	right.Normalize();

	Math::Matrix m = Math::Matrix::Identity;
	m.Right(right);
	m.Up(axis);
	m.Backward(right.Cross(axis));
	m.Translation(pos);
	return m;
}

class WindShellShader
{
public:

	WindShellShader() {}
	~WindShellShader() { Release(); }

	// コピー禁止(D3D11の生ポインタ資源を持つ為)
	WindShellShader(const WindShellShader&) = delete;
	WindShellShader& operator=(const WindShellShader&) = delete;

	bool Init();

	void Release();

	// 1レイヤーを描画する(ステートの設定と復元もここで行う)
	//	blendMode：Alpha(墨)かAdd(芯)。それ以外はAlpha扱い
	//	streakTexture：筋テクスチャ。noiseTextureが無ければ消去は線形フェード
	//	perlin：頂点ノイズ用の順列テーブル(Init済みのもの)
	//	effectLayerBlend：falseならトレイルと同じ(AlphaMasked/Add)、trueならエフェクト層用(Alpha/EffectAdd)
	void Draw(const WindShellParams& params, ParticleBlendMode blendMode,
		const std::shared_ptr<KdTexture>& streakTexture,
		const ParticleNoiseParams& noise, const std::shared_ptr<KdTexture>& noiseTexture,
		const PerlinNoiseGPU& perlin, bool effectLayerBlend = false);

	bool IsInitialized() const { return m_initialized; }

private:

	bool CreateGridMesh();

	// HLSL側と一致させる事(cbWindShell:b10 / g_streakTex:t1)。Perlin順列テーブルはKdShaderManager::PerlinPermSlotをVSへバインドする
	static constexpr UINT	kCBSlot = 10;
	static constexpr UINT	kStreakTexSlot = 1;

	// グリッドの分割数(角度 x 軸方向)
	static constexpr int	kSegU = 32;
	static constexpr int	kSegV = 24;

	bool	m_initialized = false;

	ID3D11VertexShader*	m_VS = nullptr;
	ID3D11PixelShader*	m_PS = nullptr;
	ID3D11InputLayout*	m_inputLayout = nullptr;

	ID3D11Buffer*		m_vertexBuffer = nullptr;
	ID3D11Buffer*		m_indexBuffer = nullptr;
	UINT				m_indexCount = 0;

	KdConstantBuffer<WindShellParams>	m_cb;
};
