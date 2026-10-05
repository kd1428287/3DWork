#pragma once

//====================================================================
//
// GPUパーティクル用バッファ
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// ・パーティクルデータ本体(StructuredBuffer)はインスタンスごとにここで保持する
// ・発生(Emit)／更新(Update)／描画(Draw)の実処理はGPUParticleShader(全インスタンスで
//   共有。KdShaderManager::Instance().particleShader_が唯一のインスタンス)に委譲する
//
// 【使い方】
//   ParticleBuffer particle;
//   particle.Init(10000);
//   :
//   // 毎フレーム
//   particle.Emit(emitParam, 50);      // 発生させたい時だけ呼ぶ
//   particle.Update(deltaTime);        // シミュレーション更新
//   particle.Draw(pTexture);           // 描画(3D描画パスの最後、半透明描画のタイミングで)
//
//====================================================================

// パーティクルのビルボード方式
enum class ParticleBillboardMode
{
	Normal,		// カメラ正面を向く正方形のビルボード(従来通り)
	Stretch,	// 速度方向へ伸びる板ポリ(火花・斬撃の軌跡等、速く飛ぶ表現向け)
	Beam,		// 静止したまま、固定した軸(Particle::Axis)方向へ中心から両端に伸びる板ポリ(線エフェクト向け)
};

// 初速の決め方(Layer単位)
enum class ParticleEmitDistribution
{
	Directional,	// 従来通り：baseDir軸のVelocityMin〜Maxの範囲(箱型の乱数)で決める
	RadialInPlane,	// 水平面(XZ)内でランダムな角度へ均等に飛ばす(角度はGPU側で毎粒子ごとに決定)。
	// 放射状に広がるヒットスパーク(十字/星形の線)向け
};

// パーティクルのブレンドモード
//	ParticleBuffer::Draw()の引数として渡し、GPUParticleShader側でKdBlendStateへ変換する
enum class ParticleBlendMode
{
	Add,	// 加算合成(発光系の火花・炎向け)
	Alpha,	// 半透明合成(煙・砂煙等、加算だと不自然になるもの向け)
	Multiply,
	LiquidInk,	// 墨の液体表現用：密度として描画し、ポストプロセス側でブラー+しきい値合成する(EffectDispatcher::DrawLiquid()で描画)
};

class ParticleBuffer
{
public:

	ParticleBuffer() {}
	~ParticleBuffer() { Release(); }

	// パーティクル1粒のデータ
	// ※HLSL側(inc_KdGPUParticle.hlsli の Particle構造体)とレイアウトを必ず一致させる事
	// ※SizeStart/SizeEndはColorStart/Colorと同様、寿命に応じてVS側で線形補間される想定
	struct Particle
	{
		Math::Vector3	Position;
		float			Life = 0.0f;

		Math::Vector3	Velocity;
		float			SizeStart = 0.1f;		// 発生時サイズ(旧Size)

		Math::Vector4	ColorStart = { 0.f,0.f,0.f, 1.0f };	// 発生直後の熱い色(1.0超で明るめに)
		Math::Vector4	Color = { 0.f,0.f,0.f, 1.0f };						// 冷えた後の最終色

		float			LifeMax = 1.0f;

		float			BillboardMode = 0.0f;	// ParticleBillboardMode::Normal相当
		float			StretchScale = 0.0f;	// Stretch時のみ使用：速度→伸び量の係数。Beam時は「幅」として読み替える
		float			SizeEnd = 0.0f;			// 消滅時サイズ(旧_pad を転用)

		// ※必ず末尾に追加する事(途中に挿すとHLSL側とオフセットがズレる。過去に実際発生した事故)
		Math::Vector3	Axis = { 0,0,1 };		// Beam専用：伸びる向き(固定。速度とは無関係)
		float			_padAxis = 0.0f;		// 16バイト境界合わせ
	};

	// 発生パラメータ
	struct EmitParameter
	{
		Math::Vector3	Position;

		Math::Vector3	VelocityMin = { -1,-1,-1 };
		Math::Vector3	VelocityMax = { 1, 1, 1 };

		float			SizeStartMin = 0.1f;
		float			SizeStartMax = 0.3f;
		float			SizeEndMin = 0.1f;
		float			SizeEndMax = 0.3f;

		float			LifeMin = 0.5f;
		float			LifeMax = 1.5f;

		Math::Vector4	ColorStartMin = { 2.0f, 2.0f, 1.5f, 1.0f };
		Math::Vector4	ColorStartMax = { 2.0f, 2.0f, 1.5f, 1.0f };

		Math::Vector4	ColorMin = { 0.f, 0.f, 0.f, 1.0f };
		Math::Vector4	ColorMax = { 0.f, 0.f, 0.f, 1.0f };

		ParticleBillboardMode	BillboardMode = ParticleBillboardMode::Normal;
		float					StretchScale = 0.0f;

		// ※必ず末尾に追加する事(途中に挿すとC++/HLSL双方の構造体オフセットがズレる。過去に実際発生した事故)
		ParticleEmitDistribution	Distribution = ParticleEmitDistribution::Directional;
		float						RadialSpeedMin = 1.0f;	// RadialInPlane専用：速度の大きさの範囲(最小)
		float						RadialSpeedMax = 4.0f;	// RadialInPlane専用：速度の大きさの範囲(最大)

		Math::Vector3	Axis = { 0,0,1 };	// Beam専用：伸びる向き(固定。baseDirをそのまま渡す想定)
	};

	//================================================
	// 初期化・解放
	//================================================

	// maxParticleNum：同時に存在できる最大パーティクル数
	bool Init(UINT maxParticleNum = 10000);

	void Release();

	//================================================
	// 更新・描画
	//================================================

	// パーティクルをcount個発生させる(リングバッファへ書き込み)
	void Emit(const EmitParameter& param, UINT count);

	// 1フレームぶんのシミュレーション更新(コンピュートシェーダーで実行)
	void Update(float deltaTime, const Math::Vector3& gravity = { 0.0f, -0.5f, 0.0f });

	// 描画(ビルボードとして描画)
	// ※事前にKdShaderManager::WriteCBCamera等でカメラ情報の転送が済んでいる事
	void Draw(const std::shared_ptr<KdTexture>& texture, ParticleBlendMode blendMode = ParticleBlendMode::Add);

	UINT GetMaxParticleNum() const { return maxParticleNum_; }

private:

	bool CreateBuffers(UINT maxParticleNum);

	bool initialized_ = false;

	UINT maxParticleNum_ = 0;

	//================================================
	// バッファ
	//================================================

	// パーティクル本体(CS：UAVで読み書き／VS：SRVで読み取り)
	ID3D11Buffer* particleBuffer_ = nullptr;
	ID3D11UnorderedAccessView* particleUAV_ = nullptr;
	ID3D11ShaderResourceView* particleSRV_ = nullptr;

	// 発生用リングバッファの書き込みカーソル(要素数1)
	ID3D11Buffer* emitCounterBuffer_ = nullptr;
	ID3D11UnorderedAccessView* emitCounterUAV_ = nullptr;
};