#pragma once

#include <wrl/client.h>

// 読み書きを交互に入れ替えるRTのペア
struct FluidDoubleRT
{
	KdRenderTargetPack	m_rt[2];
	int					m_read = 0;

	void Create(int w, int h, DXGI_FORMAT format)
	{
		m_rt[0].CreateRenderTarget(w, h, false, format);
		m_rt[1].CreateRenderTarget(w, h, false, format);
	}

	void Clear()
	{
		for (KdRenderTargetPack& rt : m_rt) { rt.ClearTexture(kBlackColor); }
	}

	void Release()
	{
		for (KdRenderTargetPack& rt : m_rt) { rt.m_RTTexture.reset(); }
	}

	KdRenderTargetPack& Read() { return m_rt[m_read]; }
	KdRenderTargetPack& Write() { return m_rt[m_read ^ 1]; }
	void Swap() { m_read ^= 1; }
};

// GPU版 Stable Fluids(Jos Stam)。1枚のPSをPassで切り替えて全工程を描く
class FluidSimulator
{
public:
	~FluidSimulator() { Release(); }

	// simWidth：シミュレーション格子の幅(高さは画面の縦横比から決める)
	bool Init(int simWidth = 384);
	void Release();

	// 描画フェーズから毎フレーム1回呼ぶ(GPUへ描画するのでUpdateスレッド側からは呼ばない)
	void Step(float deltaTime);

	// 次のStep()で反映する墨の入力(CPU側の記録のみなのでUpdateから呼んでよい)
	//	posUV/deltaUV … 0〜1の画面座標(Yは下が正)と、このフレームの移動量
	//	radius        … 画面の高さに対する比率
	void AddSplat(const Math::Vector2& posUV, const Math::Vector2& deltaUV, float radius, float density);

	// 濃度を墨と紙の見た目に変換し、現在のレンダーターゲット全体へ描く(背景用)
	void Draw();

	std::shared_ptr<KdTexture> GetDensityTexture() { return m_density.Read().m_RTTexture; }

private:
	// KdFluid_PS.hlslのPASS_*と値を合わせる
	enum class Pass : int
	{
		Advect = 0,
		SplatVelocity,
		SplatDensity,
		Divergence,
		Jacobi,
		Gradient,
		Curl,
		VorticityForce,
		Diffuse,
		Display,
		Coverage,
		EmitVelocity,
		EmitDensity,
		Buoyancy,
		Ambient,
	};

	struct Vertex
	{
		Math::Vector3 Pos;
		Math::Vector2 UV;
	};

	struct cbFluid
	{
		Math::Vector2 TexelSize = { 0.0f, 0.0f };	// 1 / シミュレーション解像度
		float DeltaTime = 0.0f;
		float Dissipation = 0.0f;					// 毎秒の減衰率

		float Aspect = 1.0f;						// 幅 / 高さ
		int   Pass = 0;
		float Diffusion = 0.0f;						// 濃度の拡散係数(texel²/秒)
		float Vorticity = 0.0f;						// 渦度強調の強さ(1/秒)

		Math::Vector2 SplatPos = { 0.0f, 0.0f };	// 0〜1の画面座標
		float SplatRadius = 0.05f;					// 画面の高さに対する比率
		float _blank2 = 0.0f;

		Math::Vector3 SplatValue = { 0.0f, 0.0f, 0.0f };	// 速度ならxy、濃度ならx
		float _blank3 = 0.0f;

		Math::Vector2 DstSize = { 1.0f, 1.0f };		// 占有率の計測グリッドの解像度
		float _blank4[2] = { 0.0f, 0.0f };

		float StreamCenterX = 0.5f;			// 入口の中心(0〜1)
		float StreamHalfWidth = 0.06f;		// 入口の帯の半幅(画面の高さに対する比率)
		float StreamSpeed = 0.0f;			// 入口の流速(texel/秒)
		float StreamAngle = 0.0f;			// 入口の流れる向き(ラジアン。0で真下)

		float StreamDensity = 0.0f;			// 入口の濃度
		float Gravity = 0.0f;				// 墨が下へ引かれる力(texel/秒²)
		float Time = 0.0f;					// 経過時間(秒)
		float AmbientForce = 0.0f;			// ゆるやかな水流の強さ(texel/秒²)
	};

	struct SplatRequest
	{
		Math::Vector2 Pos;
		Math::Vector2 Delta;
		float Radius = 0.0f;
		float Density = 0.0f;
	};

	// fieldをvelocityで移流する(velocity==fieldなら自己移流)
	void Advect(FluidDoubleRT& velocity, FluidDoubleRT& field, float dissipation);

	// 速度場の発散を取り除く(発散 → ヤコビ反復で圧力 → 圧力の勾配を引く)
	void Project();

	// 小さな渦が潰れないよう、渦の回転を強める
	void AddVorticity();

	// fieldを隣のマスとなじませる(墨の滲み)
	void Diffuse(FluidDoubleRT& field);

	// 上端の入口の帯の中を、ストリームの速度・濃度へ近づける(passで速度か濃度を選ぶ)
	void Emit(Pass pass, FluidDoubleRT& field);

	// 墨は水より重いので、濃度に比例した力で下へ引く
	void AddBuoyancy();

	// 画面全体へ、ゆっくり変化する弱い渦状の流れを加える
	void AddAmbientFlow();

	// 入口をノイズで不規則に動かし、濃度と太さにゆらぎを与える(太さの基準は占有率で変える)
	void UpdateStream(float dt);

	// ときどき、墨を加えない小さな力(水の乱れ)を、ランダムな位置と向きへ加える
	void UpdateDisturbance(float dt);

	// 墨の占有率を計測する(GPUで縮小してから、数フレーム遅れでCPUへ読み戻す)
	void MeasureCoverage();
	void ReadCoverage();

	// 溜めた入力を速度と濃度へ加える
	void ApplySplats(float dt);

	// fieldの1点(ガウス分布)へvalueを加える
	void Splat(FluidDoubleRT& field, Pass pass, const Math::Vector2& posUV, float radius, const Math::Vector3& value);

	// VS・PS・定数バッファをデバイスへ設定する
	void BindPipeline();

	// src0/src1をt0/t1に渡して、dstへ全画面描画する(srcはnullptr可)
	void DrawPass(Pass pass, KdTexture* src0, KdTexture* src1, KdRenderTargetPack& dst);


	// 描画先は切り替えず、現在のレンダーターゲットへ全画面描画する
	void DrawQuad(Pass pass, KdTexture* src0, KdTexture* src1);

	static constexpr float kMinDeltaTime = 1.0f / 240.0f;
	static constexpr float kMaxDeltaTime = 1.0f / 30.0f;
	static constexpr float kVelocityDissipation = 0.8f;	// 毎秒の減衰率
	static constexpr float kDensityDissipation = 0.05f;
	static constexpr float kDensityDiffusion = 6.0f;	// texel²/秒
	static constexpr float kVorticity = 3.0f;			// 1/秒
	static constexpr float kMaxSplatSpeed = 600.0f;		// texel/秒
	static constexpr int   kMaxSplatSubdivision = 16;
	static constexpr int   kJacobiIterations = 20;

	// 墨の占有率の制御
	static constexpr float kTargetCoverage = 0.12f;	// 画面に占める墨の割合の目標(0〜1)
	static constexpr int   kMeasureInterval = 4;	// 占有率を計測するフレーム間隔
	static constexpr int   kCoverageGridWidth = 32;	// 計測用の縮小グリッドの幅
	static constexpr int   kStagingCount = 3;		// 読み戻し用バッファの数

	// 画面の上端から注がれる、一本の墨のストリーム(下端から画面の外へ流れ出る)
	static constexpr float kStreamSpeed = 25.0f;			// 注ぐ勢い(texel/秒)。小さいほど静か
	static constexpr float kStreamCenterSwing = 0.2f;		// 入口の中心の左右の振れ幅(画面の幅に対する比率)
	static constexpr float kStreamCenterSpeed = 0.12f;		// 入口の中心が動く速さ(ノイズの変化/秒)
	static constexpr float kStreamDensityMin = 0.55f;		// 入口の濃度の範囲(濃淡のゆらぎ)
	static constexpr float kStreamDensityMax = 1.0f;
	static constexpr float kStreamDensitySpeed = 0.2f;		// 濃度が変わる速さ(ノイズの変化/秒)
	static constexpr float kStreamWidthVar = 0.35f;			// 帯の太さのゆらぎ(基準に対する比率)
	static constexpr float kStreamWidthVarSpeed = 0.35f;	// 太さが変わる速さ(ノイズの変化/秒)
	static constexpr float kStreamWidthMin = 0.01f;			// 帯の半幅の基準の下限(画面の高さに対する比率)
	static constexpr float kStreamWidthMax = 0.15f;			// 上限
	static constexpr float kStreamWidthGain = 0.05f;		// 占有率の不足1あたりの、半幅の基準の変化(比率/秒)
	static constexpr float kStreamWidthMaxRate = 0.02f;		// 半幅の基準の変化速度の上限(比率/秒)
	static constexpr float kCoverageFilterTime = 1.5f;		// 占有率の計測値をなめらかにする時間(秒)
	static constexpr float kCoverageDeadband = 0.01f;		// 目標との差がこの範囲なら、太さの基準を変えない
	// ランダムな力
	static constexpr float kAmbientForce = 25.0f;			// ゆるやかな水流の強さ(texel/秒²)
	static constexpr float kDisturbIntervalMin = 1.5f;		// 小さな乱れが起きる間隔(秒)
	static constexpr float kDisturbIntervalMax = 4.0f;
	static constexpr float kDisturbSpeedMin = 25.0f;		// 乱れの初速(texel/秒)
	static constexpr float kDisturbSpeedMax = 60.0f;
	static constexpr float kDisturbRadiusMin = 0.06f;		// 乱れの大きさ(画面の高さに対する比率)
	static constexpr float kDisturbRadiusMax = 0.12f;

	static constexpr float kGravity = 60.0f;				// 濃度1の墨が下へ引かれる力(texel/秒²)

	ID3D11VertexShader*	m_VS = nullptr;
	ID3D11InputLayout*	m_inputLayout = nullptr;
	ID3D11PixelShader*	m_PS = nullptr;

	KdConstantBuffer<cbFluid> m_cb;

	FluidDoubleRT	m_velocity;		// texel/秒(Yは下が正)
	FluidDoubleRT	m_density;		// 墨の濃度(0〜1)
	FluidDoubleRT	m_pressure;		// 圧力(前フレームの値から反復を始める)
	KdRenderTargetPack m_divergence;	// 速度場の発散
	KdRenderTargetPack m_curl;			// 速度場の渦度

	// 占有率の計測：濃度をブロックごとに平均した縮小グリッド → ステージング → CPU
	KdRenderTargetPack m_coverageGrid;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_coverageTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_staging[kStagingCount];
	int		m_gridWidth = 0;
	int		m_gridHeight = 0;
	int		m_stagingWrite = 0;		// 次にコピーするバッファ
	int		m_stagingRead = 0;		// 次に読み取るバッファ
	int		m_stagingPending = 0;	// コピー済みで未読のバッファ数
	int		m_measureFrame = 0;
	float	m_coverage = 0.0f;		// 最新の計測結果(0〜1)
	bool	m_coverageValid = false;

	std::vector<SplatRequest> m_splats;

	float			m_streamTime = 0.0f;
	float			m_streamHalfWidth = 0.08f;	// 占有率に応じて変わる、入口の帯の半幅の基準
	float			m_prevCenterX = -1.0f;		// 前フレームの入口の中心(未設定は負)
	float			m_coverageFiltered = 0.0f;	// なめらかにした占有率
	bool			m_coverageFilterInit = false;
	float			m_disturbTimer = 1.0f;		// 次の乱れまでの残り時間
	int				m_disturbCount = 0;			// 起きた乱れの回数(乱数の種)

	int		m_simWidth = 0;
	int		m_simHeight = 0;

	Vertex	m_screenVert[4];
};
