#include "../inc_KdCommon.hlsli"
#include "inc_KdGPUParticle.hlsli"

// パーティクル本体バッファ(読み取り専用)
StructuredBuffer<Particle> g_ParticleBuffer : register(t0);

// 板ポリの四隅のオフセット(中心からの相対位置)
static const float2 kQuadOffset[4] =
{
	float2(-0.5f, 0.5f),
	float2(0.5f, 0.5f),
	float2(-0.5f, -0.5f),
	float2(0.5f, -0.5f),
};

// 板ポリの四隅のUV座標(kQuadOffsetと対応)
static const float2 kQuadUV[4] =
{
	float2(0, 0),
	float2(1, 0),
	float2(0, 1),
	float2(1, 1),
};

//====================================================================
// ストレッチビルボード関連の固定値
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 速度→伸び量の係数(Particle::StretchScale)はLayer単位でJSON/エディタから
// 調整できるが、伸び量のクランプ範囲(sizeに対する倍率)と「静止」とみなす速度の
// 閾値は、暴走(伸びすぎ/潰れすぎ)を防ぐ安全装置的な値なのでシェーダー内の
// 固定値のままにしている。チューニングが必要になったらここを調整するか、
// 将来的にLayer単位のパラメータへ昇格させること。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
static const float kStretchMinLenMul = 1.0f;
static const float kStretchMaxLenMul = 6.0f;
static const float kStretchMinSpeed = 0.001f;

//================================
// 頂点シェーダ：頂点バッファを使わず、
// SV_VertexID(0～3)とSV_InstanceID(パーティクル番号)だけで
// カメラ方向を向いた板ポリ(ビルボード)を1枚組み立てる
//
// Particle::BillboardModeがStretch(1.0)のパーティクルは、
// スクリーン空間へ投影した速度方向に伸びる板ポリになる
//================================
VSOutput main(uint vertID : SV_VertexID, uint instID : SV_InstanceID)
{
	VSOutput Out = (VSOutput) 0;

	Particle p = g_ParticleBuffer[instID];

	// 死亡中のパーティクルは面積0にして描画結果に影響しないようにする
	float size = (p.Life > 0) ? p.Size : 0;

	// ビュー行列(row_major)の列から、カメラのワールド空間での右方向・上方向を取り出す
	// ※mul(v, mView)というベクトル×行列の掛け方をしている場合、
	//   ビュー行列の各「列」がカメラのワールド空間での軸ベクトルになる
	float3 camRight = float3(g_mView._11, g_mView._21, g_mView._31);
	float3 camUp = float3(g_mView._12, g_mView._22, g_mView._32);

	// 板ポリのローカル軸(camRight/camUp平面上の2D方向)と、幅・高さの長さ(sizeと同じ単位)
	// デフォルトはNormal(カメラ正面の正方形)のままの値
	float2 quadRightAxis = float2(1, 0);
	float2 quadUpAxis = float2(0, 1);
	float quadWidthLen = size;
	float quadHeightLen = size;

	// BillboardMode > 0.5 は Stretch(KdParticleBillboardMode::Stretch相当)
	if (p.BillboardMode > 0.5f)
	{
		// 速度をスクリーン(camRight/camUp)平面へ投影する。
		// camRight/camUp/カメラ視線方向は正規直交基底なので、この2成分が
		// そのままスクリーン空間での速度ベクトルになる
		float2 screenVel = float2(dot(p.Velocity, camRight), dot(p.Velocity, camUp));
		float screenSpeed = length(screenVel);

		if (screenSpeed > kStretchMinSpeed)
		{
			// 速度方向を新しい上方向(伸びる方向)、それと直交する方向を新しい右方向にする
			// ※(quadRightAxis, quadUpAxis)の向き(行列式の符号)を元の(camRight, camUp)と
			//   揃えないと、板ポリの巻き順が反転して背面カリングで消えてしまう点に注意
			//   (float2(-y, x)ではなくfloat2(y, -x)にすることで向きを保っている)
			quadUpAxis = screenVel / screenSpeed;
			quadRightAxis = float2(quadUpAxis.y, -quadUpAxis.x);

			float stretchedLen = size * kStretchMinLenMul + screenSpeed * p.StretchScale;
			quadHeightLen = clamp(stretchedLen, size * kStretchMinLenMul, size * kStretchMaxLenMul);
			// 幅(quadWidthLen)は変えない。速いほど鋭く見せたい場合はここをscreenSpeedに応じて狭める
		}
		// screenSpeedが閾値以下(ほぼ静止)の場合は、初期値(通常の正方形ビルボード)の
		// ままにして自然にフォールバックさせる
	}

	float3 stretchRight = camRight * quadRightAxis.x + camUp * quadRightAxis.y;
	float3 stretchUp = camRight * quadUpAxis.x + camUp * quadUpAxis.y;

	float3 wPos = p.Position
		+ stretchRight * kQuadOffset[vertID].x * quadWidthLen
		+ stretchUp * kQuadOffset[vertID].y * quadHeightLen;

	Out.Pos = mul(float4(wPos, 1), g_mView);
	Out.Pos = mul(Out.Pos, g_mProj);

	Out.UV = kQuadUV[vertID];
	Out.LifeRate = (p.LifeMax > 0) ? saturate(p.Life / p.LifeMax) : 0;

	// 発生直後(LifeRate=1)はColorStart、消滅直前(LifeRate=0)はColorへ補間
	Out.Color = lerp(p.ColorStart, p.Color, 1 - Out.LifeRate);

	return Out;
}
