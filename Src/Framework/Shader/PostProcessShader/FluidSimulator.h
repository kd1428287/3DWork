#pragma once

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

	std::shared_ptr<KdTexture> GetDensityTexture() { return m_density.Read().m_RTTexture; }

private:
	// KdFluid_PS.hlslのPASS_*と値を合わせる
	enum class Pass : int
	{
		Advect = 0,
		SplatVelocity,
		SplatDensity,
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
		float _blank1[2] = { 0.0f, 0.0f };

		Math::Vector2 SplatPos = { 0.0f, 0.0f };	// 0〜1の画面座標
		float SplatRadius = 0.05f;					// 画面の高さに対する比率
		float _blank2 = 0.0f;

		Math::Vector3 SplatValue = { 0.0f, 0.0f, 0.0f };	// 速度ならxy、濃度ならx
		float _blank3 = 0.0f;
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

	// 溜めた入力を速度と濃度へ加える
	void ApplySplats(float dt);

	// fieldの1点(ガウス分布)へvalueを加える
	void Splat(FluidDoubleRT& field, Pass pass, const Math::Vector2& posUV, float radius, const Math::Vector3& value);

	// src0/src1をt0/t1に渡して、dstへ全画面描画する(srcはnullptr可)
	void DrawPass(Pass pass, KdTexture* src0, KdTexture* src1, KdRenderTargetPack& dst);

	static constexpr float kMinDeltaTime = 1.0f / 240.0f;
	static constexpr float kMaxDeltaTime = 1.0f / 30.0f;
	static constexpr float kVelocityDissipation = 1.0f;
	static constexpr float kDensityDissipation = 0.1f;
	static constexpr float kMaxSplatSpeed = 600.0f;		// texel/秒
	static constexpr int   kMaxSplatSubdivision = 16;

	ID3D11VertexShader*	m_VS = nullptr;
	ID3D11InputLayout*	m_inputLayout = nullptr;
	ID3D11PixelShader*	m_PS = nullptr;

	KdConstantBuffer<cbFluid> m_cb;

	FluidDoubleRT	m_velocity;		// texel/秒(Yは下が正)
	FluidDoubleRT	m_density;		// 墨の濃度(0〜1)

	std::vector<SplatRequest> m_splats;

	int		m_simWidth = 0;
	int		m_simHeight = 0;

	Vertex	m_screenVert[4];
};
