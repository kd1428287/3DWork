#include "inc_GPUParticleShader.hlsli"

// パーティクル本体バッファ(読み書き)
RWStructuredBuffer<Particle> g_ParticleBuffer : register(u0);

cbuffer cbGPUParticleEmit : register(b0)
{
	float3 g_EmitPos; // 発生座標
	int g_EmitCount; // 今回発生させる数

	// ※vec3とfloatを交互に並べ、C++側cbEmitと同じ順序・サイズを保つ事(16バイト境界のズレ防止)
	float3 g_EmitVelocityMin; // 初速の範囲(最小)
	float g_EmitSizeStartMin; // サイズの範囲(最小)

	float3 g_EmitVelocityMax; // 初速の範囲(最大)
	float g_EmitSizeStartMax; // サイズの範囲(最大)

	float4 g_EmitColorStartMin; // 色
	float4 g_EmitColorStartMax; // 色
	
	float4 g_EmitColorMin; // 色
	float4 g_EmitColorMax; // 色

	float g_EmitLifeMin; // 寿命の範囲(最小)
	float g_EmitLifeMax; // 寿命の範囲(最大)
	uint g_MaxParticleNum; // パーティクルの最大数
	float g_RandomSeed; // 乱数シード(毎回変える事)

	// ストレッチビルボード関連(C++側 KdGPUParticle::cbEmit とレイアウトを一致させる事)
	float g_EmitBillboardMode; // 0:Normal 1:Stretch(そのままParticle::BillboardModeへコピー)
	float g_EmitStretchScale; // Stretch時のみ使用：速度→伸び量の係数
	float g_EmitSizeEndMin;
	float g_EmitSizeEndMax;

	// 初速の決め方(必ず末尾に追加する事)
	float g_EmitDistribution; // 0:Directional 1:RadialInPlane 2:Converge
	float g_EmitRadialSpeedMin;
	float g_EmitRadialSpeedMax;
	float _padDistribution;

	float3 g_EmitAxis; // Beam専用：伸びる向き(固定)
	float _padAxis;

	float g_EmitConvergeRadius;
	float g_EmitConvergeSpeed;
	uint g_EmitStartIndex; // リングバッファの書き込み開始位置(C++側で管理)
	float _padEmit;
};

// 初速の決め方(C++側enumと値を一致させる事)
static const int EMIT_DIST_DIRECTIONAL = 0;
static const int EMIT_DIST_RADIAL_IN_PLANE = 1;
static const int EMIT_DIST_CONVERGE = 2;

//================================
// 新規パーティクルの発生
// ※スレッド数はC++側でEmitCountぶん(256の倍数に切り上げ)Dispatchする
// ※リングバッファ方式：最大数を超えたら一番古い(生きていても)パーティクルから上書きする
//================================
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	// 最大数を超える発生は切り捨てる(同一indexへの書き込み衝突を防ぐ)
	uint emitCount = min((uint) max(g_EmitCount, 0), g_MaxParticleNum);
	if (id.x >= emitCount)
	{
		return;
	}

	// 開始位置からの連番で書き込み先を決める(アトミック不要)
	uint index = (g_EmitStartIndex + id.x) % g_MaxParticleNum;

	// スレッド毎に異なる乱数シードを作る
	float seed = g_RandomSeed + (float) id.x * 0.918273f;

	// 未代入フィールドが混ざらないよう0初期化
	Particle p = (Particle) 0;
	p.Position = g_EmitPos;

	// float渡しのモード値を丸めて整数化
	int distribution = (int) (g_EmitDistribution + 0.5f);

	// 寿命(LifeMax)を先に決定（Convergeモードで速度を自動計算する際に必要なため）
	p.LifeMax = RandRange(g_EmitLifeMin, g_EmitLifeMax, seed + 4.0f);
	p.Life = p.LifeMax;

	if (distribution == EMIT_DIST_RADIAL_IN_PLANE)
	{
		// 水平面(XZ)内でランダムな角度へ均等に飛ばす(放射状ヒットスパーク向け)
		float angle = RandRange(0.0f, 6.28318530718f, seed + 8.0f);
		float speed = RandRange(g_EmitRadialSpeedMin, g_EmitRadialSpeedMax, seed + 9.0f);
		p.Velocity = float3(cos(angle), 0.0f, sin(angle)) * speed;
	}
	else if (distribution == EMIT_DIST_CONVERGE)
	{
		// ------------------------------------------------------------------
		// 周囲から中心(g_EmitPos)へ向かって集まる処理
		// ------------------------------------------------------------------
		
		// 1. 単位球上のランダムな方向(dir)を計算 (全方位へ均等散布)
		float z = RandRange(-1.0f, 1.0f, seed + 8.0f);
		float phi = RandRange(0.0f, 6.28318530718f, seed + 9.0f);
		float rXY = sqrt(max(0.0f, 1.0f - z * z));
		float3 dir = float3(rXY * cos(phi), rXY * sin(phi), z);

		// 2. 発生位置を中心から ConvergeRadius 離れた球面上に設定
		p.Position = g_EmitPos + (dir * g_EmitConvergeRadius);

		// 3. 初速（中心に向かうベクトル）の設定
		float speed = g_EmitConvergeSpeed;
		if (speed <= 0.0f)
		{
			// 速度が指定されていない(<=0)場合は、寿命(LifeMax)でピッタリ中心に到達するように自動計算
			speed = g_EmitConvergeRadius / max(p.LifeMax, 0.0001f);
		}

		p.Velocity = -dir * speed;
	}
	else
	{
		p.Velocity = RandRange3(g_EmitVelocityMin, g_EmitVelocityMax, seed);
	}

	p.SizeStart = RandRange(g_EmitSizeStartMin, g_EmitSizeStartMax, seed + 3.0f);
	p.SizeEnd = RandRange(g_EmitSizeEndMin, g_EmitSizeEndMax, seed + 7.0f);
	p.ColorStart = RandRange4(g_EmitColorStartMin, g_EmitColorStartMax, seed + 5.0f);
	p.Color = RandRange4(g_EmitColorMin, g_EmitColorMax, seed + 6.0f);
	p.BillboardMode = g_EmitBillboardMode;
	p.StretchScale = g_EmitStretchScale;
	p.Axis = g_EmitAxis;
	p.Distribution = g_EmitDistribution;
	p.ConvergeCenter = g_EmitPos;

	g_ParticleBuffer[index] = p;
}
