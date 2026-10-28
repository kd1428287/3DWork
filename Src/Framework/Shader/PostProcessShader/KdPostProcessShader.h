#pragma once

// 液体表現の質感の種類(合成PSの切り替え用。血・水はここへ追加する)
enum class LiquidStyle
{
	Ink,
};

class KdPostProcessShader
{
public:
	KdPostProcessShader() {}
	~KdPostProcessShader()
	{
		Release();
	}

	void SetNearClippingDistance(float distance) { m_cb0_DoFInfo.Work().NearClippingDistance = distance; }
	void SetFarClippingDistance(float distance) { m_cb0_DoFInfo.Work().FarClippingDistance = distance; }
	void SetFocusDistance(float distance) { m_cb0_DoFInfo.Work().FocusDistance = distance; }
	void SetFocusRange(float fore, float back) { m_cb0_DoFInfo.Work().FocusForeRange = fore; m_cb0_DoFInfo.Work().FocusBackRange = back; }

	void SetBrightThreshold(float threshold) { m_cb0_BrightInfo.Work().Threshold = threshold; }

	void SetExposure(float exposure) { m_cb0_ColorGradeInfo.Work().Exposure = exposure; }
	void SetContrast(float contrast) { m_cb0_ColorGradeInfo.Work().Contrast = contrast; }
	void SetSaturation(float saturation) { m_cb0_ColorGradeInfo.Work().Saturation = saturation; }
	// temperature : -1(寒色/青) ～ +1(暖色/オレンジ)
	void SetTemperature(float temperature) { m_cb0_ColorGradeInfo.Work().Temperature = temperature; }
	// tint : -1(緑) ～ +1(マゼンタ)
	void SetTint(float tint) { m_cb0_ColorGradeInfo.Work().Tint = tint; }

	struct Vertex
	{
		Math::Vector3 Pos;
		Math::Vector2 UV;
	};

	bool Init();

	void Release();

	void Draw();

	void BeginBright();
	void EndBright();

	// 液体表現(密度RT→ブラー→しきい値合成)。BeginとEndの間で、液体用ParticleBufferをAddで描画する。
	// 本編のシーンRT(MRT)がバインドされている3D描画中に呼ぶこと
	// useSceneDepth=false：シーン深度を共有しない(エディタのプレビュー等、別RTへ描く場合)
	void BeginLiquid(bool useSceneDepth = true);
	void EndLiquid(LiquidStyle style = LiquidStyle::Ink);

	// エフェクト層：BeginとEndの間に描いたパーティクル等は、シーンとは別のRT(RGB=乗算済みアルファ、A=被覆率)へ溜められ、
	// カラーグレードの後に合成される(=グレード・DoFの対象外)。深度はシーンのものを共有する。
	// 本編のシーンRTがバインドされている3D描画中に呼ぶこと
	void BeginEffectLayer();
	void EndEffectLayer();
	bool IsEffectLayerActive() const { return m_effectLayerActive; }

	// falseにすると、エフェクト層を使わず従来どおりシーンRTへ直接描く(不具合の切り分け用)
	void SetEffectLayerEnabled(bool enabled) { m_effectLayerEnabled = enabled; }

	// エディタプレビュー等、呼び出し側が自前のエフェクト層RTをバインドして描く間だけtrueにする
	//	(RTの切り替えはせず、パーティクルのブレンド選択だけをエフェクト層用にする)
	void SetExternalEffectLayerActive(bool active) { m_effectLayerActive = active; }

	// 衝撃波(パリィ等)。centerUVは0〜1の画面座標、maxRadiusは画面の高さに対する比率
	void AddShockwave(const Math::Vector2& centerUV, float duration, float maxRadius, float strength);
	// ワールド座標指定版(カメラの背後なら何もしない)
	void AddShockwaveWorld(const Math::Vector3& worldPos, float duration, float maxRadius, float strength);
	// ワールド座標を画面UV(0〜1)へ変換する。カメラの背後ならfalse
	bool WorldToScreenUV(const Math::Vector3& worldPos, Math::Vector2& outUV) const;
	// 毎フレームPostEffectProcess()より前に呼ぶ(渡すdeltaTimeでヒットストップの影響を制御)
	void UpdateDistortion(float deltaTime);

	void SetShockwaveWidth(float width) { m_cb0_DistortionInfo.Work().ShockwaveWidth = width; }
	void SetChromaticAmount(float amount) { m_cb0_DistortionInfo.Work().ChromaticAmount = amount; }

	// 質感テクスチャ(グレー、0.5=変化なし。nullptrで無効)
	void SetSurfaceTexture(std::shared_ptr<KdTexture> tex) { m_surfaceTex = tex; }
	void SetSurfaceIntensity(float intensity) { m_cb0_SurfaceInfo.Work().Intensity = intensity; }
	// 質感が効く明るさの範囲(low以下で最大、high以上で消える)
	void SetSurfaceLumaRange(float low, float high) { m_cb0_SurfaceInfo.Work().LumaLow = low; m_cb0_SurfaceInfo.Work().LumaHigh = high; }
	void SetSurfaceTransform(const Math::Vector2& tiling, const Math::Vector2& offset)
	{
		m_cb0_SurfaceInfo.Work().Tiling = tiling;
		m_cb0_SurfaceInfo.Work().Offset = offset;
	}
	// intervalごとにOffsetをランダムに切り替える(0で無効)
	void SetSurfaceJitter(float interval) { m_surfaceJitterInterval = interval; }
	// 毎フレームPostEffectProcess()より前に呼ぶ(ジッター用)
	void UpdateSurface(float deltaTime);

	// 画面遷移(波紋)。進行管理はTransitionController側で、ここは描画パラメータのみ持つ
	//	・progress … 0=通常、1=完全に覆われた
	//	・centerUV … 波紋の中心(0〜1の画面座標)
	// 毎フレームPostEffectProcess()より前に呼ぶ
	void SetTransition(float progress, const Math::Vector2& centerUV, const Math::Vector3& color, float time);
	// 見た目の調整(歪みの強さ・波の細かさ・帯の幅・色収差・覆いの縁の柔らかさ)
	void SetTransitionWave(float amount, float freq, float ringWidth, float chromatic, float softness);
	// UI描画後に呼ぶ。progress>0の間だけ画面全体(UI含む)を覆う
	void DrawTransitionCover();

	// 水墨画化(リザルト演出)。progress=0で無効、1で画面全体。centerUVは墨の広がる起点
	// 毎フレームPostEffectProcess()より前に呼ぶ
	void SetInkWash(float progress, const Math::Vector2& centerUV, float time);

	void PostEffectProcess();

	// モーションブラー(カメラ移動由来。深度から前フレーム位置を逆算してぼかす)
	void SetMotionBlurEnabled(bool enabled) { m_motionBlurEnabled = enabled; }
	// カメラ移動由来のブラーの強さ(シャッター係数)。0でカメラブラーなし
	void SetMotionBlurStrength(float strength) { m_cb0_MotionBlurInfo.Work().Strength = strength; }
	// キャラクター(スキンメッシュ)自身の動きによるブラーの強さ。0でなし
	void SetMotionBlurObjectStrength(float strength) { m_cb0_MotionBlurInfo.Work().ObjStrength = strength; }
	void SetMotionBlurSampleNum(int num) { m_cb0_MotionBlurInfo.Work().SampleNum = (std::max)(4, (std::min)(num, 32)); }
	// ブラー長の上限(画面UV比)。カメラカット時の暴れ防止
	void SetMotionBlurMaxLength(float uv) { m_cb0_MotionBlurInfo.Work().MaxBlurUV = uv; }
	// カメラが瞬間移動した時(シーン切替・カット)に呼ぶ
	void ResetMotionBlurHistory();

	// エディタプレビュー等、本編以外の任意サイズテクスチャに対してカラーグレードのみを適用する。
	// 本編のColorGradeProcess()と同じVS/PS/定数バッファ(パラメータも共通)をそのまま使い回す。
	//	・srcColor … グレーディング元のカラー(プレビュー用オフスクリーン等)
	//	・srcMask  … カラーグレード除外マスク入力(t1)。本編と違いこの用途向けの専用マスクを
	//	  持たない場合は、呼び出し側で1x1の黒ダミーテクスチャを用意して渡すこと(黒=グレーディング適用)
	//	・dstColor … 結果の書き込み先。srcColorとは別のテクスチャであること(同一不可)
	//	・srcEffect … エフェクト層(rgb=乗算済みアルファ、a=被覆率)。指定するとグレード対象外として結果に重ねる(省略可)
	void ApplyColorGradeOnly(std::shared_ptr<KdTexture> srcColor, std::shared_ptr<KdTexture> srcMask,
		std::shared_ptr<KdTexture> dstColor, D3D11_VIEWPORT* pVP, std::shared_ptr<KdTexture> srcEffect = nullptr);

	void GenerateBlurTexture(std::shared_ptr<KdTexture>& spSrcTex, std::shared_ptr<KdTexture>& spDstTex, D3D11_VIEWPORT& VP, int blurRadius);

private:

	void BlurProcess();
	void LightBloomProcess();
	void DepthOfFieldProcess();
	// 描画したらtrue(結果は m_motionBlurRTPack)
	bool MotionBlurProcess(const std::shared_ptr<KdTexture>& srcTex);
	void ColorGradeProcess(const std::shared_ptr<KdTexture>& srcTex);
	void DistortionProcess();
	void TransitionWarpProcess(const std::shared_ptr<KdTexture>& srcTex);
	void InkWashProcess(const std::shared_ptr<KdTexture>& srcTex);
	void SurfaceProcess(const std::shared_ptr<KdTexture>& srcTex);

	void CreateBlurOffsetList(std::vector<Math::Vector3>& dstInfo, const std::shared_ptr<KdTexture>& spSrcTex, int samplingSize, const Math::Vector2& dir);

	void DrawTexture(std::shared_ptr<KdTexture>* spSrcTex, int srcTexSize, std::shared_ptr<KdTexture> spDstTex, D3D11_VIEWPORT* pVP);

	void SetBlurInfo(const std::shared_ptr<KdTexture>& spSrcTex, int samplingSize, const Math::Vector2& dir);
	void SetBlurInfo(const std::vector<Math::Vector3>& srcInfo);

	void SetBlurToDevice();
	void SetDoFToDevice();
	void SetBrightToDevice();

	ID3D11VertexShader* m_VS = nullptr;
	ID3D11InputLayout* m_inputLayout = nullptr;

	ID3D11PixelShader* m_PS_Blur = nullptr;
	ID3D11PixelShader* m_PS_DoF = nullptr;
	ID3D11PixelShader* m_PS_Bright = nullptr;
	ID3D11PixelShader* m_PS_ColorGrade = nullptr;
	ID3D11PixelShader* m_PS_LiquidInk = nullptr;
	ID3D11PixelShader* m_PS_Distortion = nullptr;
	ID3D11PixelShader* m_PS_Surface = nullptr;
	ID3D11PixelShader* m_PS_TransitionWarp = nullptr;
	ID3D11PixelShader* m_PS_TransitionCover = nullptr;
	ID3D11PixelShader* m_PS_InkWash = nullptr;
	ID3D11PixelShader* m_PS_MotionBlur = nullptr;

	static const int kBlurSamplingRadius = 8;
	static const int kLightBloomSamplingRadius = 4;

	static const int kMaxSampling = 31;
	struct cbBlur
	{
		Math::Vector4 Info[kMaxSampling];

		int SamplingNum = 0;
		int _blank[3] = { 0, 0 ,0 };
	};
	KdConstantBuffer<cbBlur>	m_cb0_BlurInfo;

	struct cbDepthOfField
	{
		float NearClippingDistance = 0.0f;
		float FarClippingDistance = 1000.0f;

		float FocusDistance = 0.0f;
		float FocusForeRange = 0.0f;
		float FocusBackRange = 1000.0f;
		int   _blank[3] = { 0, 0, 0 };
	};
	KdConstantBuffer<cbDepthOfField>	m_cb0_DoFInfo;

	struct cbBrightFilter
	{
		float Threshold = 0.0f;
		int _blank[3] = { 0, 0, 0 };
	};
	KdConstantBuffer<cbBrightFilter>	m_cb0_BrightInfo;

	struct cbColorGradeInfo
	{
		float Exposure = 1.0f;
		float Contrast = 1.0f;
		float Saturation = 1.0f;
		float Temperature = 0.0f; // -1(寒色/青) ～ +1(暖色/オレンジ)

		float Tint = 0.0f; // -1(緑) ～ +1(マゼンタ)
		int   _blank[3] = { 0, 0, 0 };
	};
	KdConstantBuffer<cbColorGradeInfo> m_cb0_ColorGradeInfo;

	struct cbLiquidInfo
	{
		Math::Vector3 InkColor = { 0.10f, 0.10f, 0.11f };	// 塊の中心の色(淡墨)
		float Threshold = 0.35f;							// 密度のしきい値(大きいほど塊が痩せる)

		Math::Vector3 EdgeColor = { 0.01f, 0.01f, 0.01f };	// 縁の色(濃墨)
		float Softness = 0.05f;								// しきい値の境界の柔らかさ

		float EdgeWidth = 0.15f;							// 縁として濃くなる密度の幅
		float HaloAlpha = 0.25f;							// しきい値の外側に出す滲みの濃さ
		float _blank[2] = { 0, 0 };
	};
	KdConstantBuffer<cbLiquidInfo> m_cb0_LiquidInfo;

	static const int kMaxShockwave = 4;
	struct cbDistortionInfo
	{
		float ChromaticAmount = 0.6f;	// 歪みの強い場所ほどRGBをずらす割合
		float ShockwaveWidth = 0.08f;	// リングの幅(画面の高さに対する比率)
		float Aspect = 1.0f;			// 画面の幅/高さ(円形補正用)
		int   ShockwaveNum = 0;			// 有効な衝撃波の数

		Math::Vector4 Shockwave[kMaxShockwave];	// xy=中心UV z=現在の半径 w=現在の強さ
	};
	KdConstantBuffer<cbDistortionInfo> m_cb0_DistortionInfo;

	struct cbSurfaceInfo
	{
		Math::Vector2 Tiling = { 1.0f, 1.0f };
		Math::Vector2 Offset = { 0.0f, 0.0f };

		float Intensity = 0.5f;
		float LumaLow = 0.4f;
		float LumaHigh = 1.2f;
		float _blank = 0.0f;
	};
	KdConstantBuffer<cbSurfaceInfo> m_cb0_SurfaceInfo;

	// 歪みPS・覆いPSで共有する(HLSL側のcbuffer cbTransitionと並びを一致させること)
	struct cbTransitionInfo
	{
		Math::Vector2 Center = { 0.5f, 0.5f };	// 波紋の中心UV
		float Progress = 0.0f;					// 0=通常 1=完全に覆われた
		float Time = 0.0f;						// 波のアニメーション用

		Math::Vector3 FadeColor = { 0.0f, 0.0f, 0.0f };
		float Aspect = 1.0f;					// 画面の幅/高さ(円形補正用)

		float MaxDist = 1.0f;					// 中心から最遠の角までの距離(アスペクト補正後)
		float WaveAmount = 0.03f;				// 歪みの強さ
		float WaveFreq = 40.0f;					// 波の細かさ
		float RingWidth = 0.12f;				// 歪みが強い帯の幅(正規化距離)

		float ChromaticAmount = 0.5f;			// 歪みの強い場所ほどRGBをずらす割合
		float Softness = 0.3f;					// 覆いの縁の柔らかさ(正規化距離)
		float _blank[2] = { 0, 0 };
	};
	KdConstantBuffer<cbTransitionInfo> m_cb0_TransitionInfo;

	// 水墨画化(HLSL側のcbuffer cbInkWashと並びを一致させること)
	struct cbInkWashInfo
	{
		Math::Vector2 Center = { 0.5f, 0.5f };	// 墨の広がる起点UV
		float Progress = 0.0f;					// 0=通常 1=画面全体
		float Time = 0.0f;						// ノイズのアニメーション用

		Math::Vector3 InkColor = { 0.06f, 0.06f, 0.07f };
		float Aspect = 1.0f;					// 画面の幅/高さ(円形補正用)

		Math::Vector3 PaperColor = { 0.93f, 0.91f, 0.86f };
		float MaxDist = 1.0f;					// 中心から最遠の角までの距離(アスペクト補正後)

		float Softness = 0.25f;					// 広がりの縁の柔らかさ
		float Contrast = 2.0f;					// 明暗の強さ
		float Pivot = 0.4f;						// 紙と墨の境になる輝度
		float Bleed = 0.25f;					// にじみ(濃淡の揺れ)

		float EdgeStrength = 0.9f;				// 輪郭線の濃さ
		float DepthEdgeGain = 8.0f;				// 深度の段差を輪郭にする強さ
		float Vignette = 0.7f;					// 周辺の暗さ
		float PaperGrain = 0.05f;				// 紙の粒状感

		float Posterize = 0.4f;					// 濃淡を段にする割合
		float NoiseScale = 4.0f;				// にじみノイズの細かさ
		float _blank[2] = { 0, 0 };
	};
	KdConstantBuffer<cbInkWashInfo> m_cb0_InkWashInfo;

	// モーションブラー(HLSL側のcbuffer cbMotionBlurと並びを一致させること)
	struct cbMotionBlurInfo
	{
		Math::Matrix Reproject;			// 現在クリップ→前フレームクリップ

		float Strength = 0.5f;			// シャッター係数
		int   SampleNum = 12;			// サンプル数
		float MaxBlurUV = 0.05f;		// ブラー長の上限(画面UV比)
		float ObjStrength = 1.0f;		// 速度バッファ(キャラクター)由来のブラーの強さ
	};
	KdConstantBuffer<cbMotionBlurInfo> m_cb0_MotionBlurInfo;

	// 発生中の衝撃波(経過時間から半径と強さを毎フレーム算出する)
	struct ShockwaveInstance
	{
		Math::Vector2 Center;
		float Age = 0.0f;
		float Duration = 0.0f;
		float MaxRadius = 0.0f;
		float Strength = 0.0f;
		bool  Active = false;
	};
	ShockwaveInstance m_shockwaves[kMaxShockwave];

public:
	//================================================
	// 現在値の取得(GUI表示用)
	//	※ ShaderTuningEditor から現在値をミラー無しで直接参照するために追加
	//================================================
	const cbDepthOfField& GetDoFCB()        const { return m_cb0_DoFInfo.Get(); }
	const cbBrightFilter& GetBrightCB()     const { return m_cb0_BrightInfo.Get(); }
	const cbColorGradeInfo& GetColorGradeCB() const { return m_cb0_ColorGradeInfo.Get(); }
	const cbLiquidInfo& GetLiquidCB() const { return m_cb0_LiquidInfo.Get(); }
	cbLiquidInfo& WorkLiquidCB() { return m_cb0_LiquidInfo.Work(); }
	const cbDistortionInfo& GetDistortionCB() const { return m_cb0_DistortionInfo.Get(); }
	const cbSurfaceInfo& GetSurfaceCB() const { return m_cb0_SurfaceInfo.Get(); }
	cbSurfaceInfo& WorkSurfaceCB() { return m_cb0_SurfaceInfo.Work(); }
	const cbTransitionInfo& GetTransitionCB() const { return m_cb0_TransitionInfo.Get(); }
	const cbInkWashInfo& GetInkWashCB() const { return m_cb0_InkWashInfo.Get(); }
	const cbMotionBlurInfo& GetMotionBlurCB() const { return m_cb0_MotionBlurInfo.Get(); }
	cbInkWashInfo& WorkInkWashCB() { return m_cb0_InkWashInfo.Work(); }

private:
	KdRenderTargetPack m_colorGradeRTPack; // 最終カラーグレーディング用RT

	KdRenderTargetPack	m_postEffectRTPack;

	// カラーグレード除外マスク(R8_UNORM。0=グレーディング適用、1=完全除外)。
	// パーティクル等、Alphaブレンドで描画されるオブジェクトのうち
	// 「露出・彩度・色温度等のスタイライズを免れたい」ピクセルだけが、
	// 描画時にPS側から能動的に書き込む(通常のLit/UnLit描画は
	// この出力スロットを一切使わないため、0のクリア値のまま残る)。
	KdRenderTargetPack	m_colorGradeMaskRTPack;

	KdRenderTargetPack	m_blurRTPack;
	KdRenderTargetPack	m_strongBlurRTPack;

	KdRenderTargetPack	m_depthOfFieldRTPack;

	KdRenderTargetPack	m_brightEffectRTPack;
	static const int	kLightBloomNum = 4;
	KdRenderTargetPack	m_lightBloomRTPack[kLightBloomNum];

	KdRenderTargetChanger m_postEffectRTChanger;
	KdRenderTargetChanger m_brightRTChanger;
	KdRenderTargetChanger m_liquidRTChanger;

	// エフェクト層(カラーグレードの後に合成するため、シーンとは別に描く)
	KdRenderTargetPack	m_effectRTPack;
	KdRenderTargetChanger m_effectRTChanger;
	bool m_effectLayerActive = false;
	bool m_effectLayerEnabled = true;

	// 液体の密度RT(シーンの深度バッファを共有するためフル解像度)と、そのぼかし結果(半解像度)
	KdRenderTargetPack	m_liquidDensityRTPack;
	KdRenderTargetPack	m_liquidBlurRTPack;

	// ディストーション適用後の最終画像(衝撃波が有効な間だけ使う)
	KdRenderTargetPack	m_distortionRTPack;

	// 画面遷移の波紋適用後の画像(遷移中だけ使う)
	KdRenderTargetPack	m_transitionRTPack;

	// 水墨画化後の画像(リザルト演出中だけ使う)
	KdRenderTargetPack	m_inkWashRTPack;

	// モーションブラー適用後の画像と、前フレームのView×Proj
	KdRenderTargetPack	m_motionBlurRTPack;
	KdRenderTargetPack	m_velocityRTPack;	// 速度バッファ(xy=UV速度 w=書き込み済みの印)
	Math::Matrix m_prevViewProj;
	bool m_hasPrevViewProj = false;
	bool m_motionBlurEnabled = false;

	// 質感合成後の最終画像と、合成する質感テクスチャ
	KdRenderTargetPack m_surfaceRTPack;
	std::shared_ptr<KdTexture> m_surfaceTex;

	float m_surfaceJitterInterval = 0.0f;
	float m_surfaceJitterTimer = 0.0f;

	Vertex m_screenVert[4];
};