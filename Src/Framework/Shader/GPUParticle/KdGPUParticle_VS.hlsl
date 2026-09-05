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
// ストレッチビルボード：見た目検証用の暫定実装
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 【現状の位置づけ(段階的実装の第2段階)】
//   Layer単位のパラメータ(BillboardMode/StretchScale等)はまだGPUParticleLayer/
//   JSON/エディタUIに配線していない。このシェーダーだけで見た目を先に固めるため、
//   全パーティクルに対して「スクリーン空間での速度が閾値を超えていれば自動的に
//   伸ばす」という暫定ロジックにしている(速度がほぼ無い場合は自然に通常の
//   正方形ビルボードへフォールバックするので、静止系のエフェクトへの影響は無い)。
//
//   ENABLE_STRETCH_BILLBOARD_TESTを0にすれば、以前の挙動(常にカメラ正面の
//   正方形)に一発で戻せる。
//
// 【後続の本配線で置き換える想定】
//   ・kStretchScale等の固定値      → GPUParticleLayerの数値(Emit時にPerParticleへコピー)
//   ・「速度がある時は常にStretch」 → Particle::BillboardMode(Normal/Stretch)による明示的な分岐
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
#define ENABLE_STRETCH_BILLBOARD_TEST 1

#if ENABLE_STRETCH_BILLBOARD_TEST
// 速度→伸び量の係数、伸び量のクランプ(sizeに対する倍率)、
// 「静止」とみなしてNormal billboardにフォールバックする速度の閾値
static const float kStretchScale = 0.15f;
static const float kStretchMinLenMul = 1.0f;
static const float kStretchMaxLenMul = 6.0f;
static const float kStretchMinSpeed = 0.001f;
#endif

//================================
// 頂点シェーダ：頂点バッファを使わず、
// SV_VertexID(0～3)とSV_InstanceID(パーティクル番号)だけで
// カメラ方向を向いた板ポリ(ビルボード)を1枚組み立てる
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
	float2 quadRightAxis = float2(1, 0);
	float2 quadUpAxis = float2(0, 1);
	float quadWidthLen = size;
	float quadHeightLen = size;

#if ENABLE_STRETCH_BILLBOARD_TEST
	// 速度をスクリーン(camRight/camUp)平面へ投影する。
	// camRight/camUp/カメラ視線方向は正規直交基底なので、この2成分が
	// そのままスクリーン空間での速度ベクトルになる
	float2 screenVel = float2(dot(p.Velocity, camRight), dot(p.Velocity, camUp));
	float screenSpeed = length(screenVel);

	if (screenSpeed > kStretchMinSpeed)
	{
		// 速度方向を新しい上方向(伸びる方向)、それと直交する方向を新しい右方向にする
		quadUpAxis = screenVel / screenSpeed;
		quadRightAxis = float2(-quadUpAxis.y, quadUpAxis.x);

		float stretchedLen = size * kStretchMinLenMul + screenSpeed * kStretchScale;
		quadHeightLen = clamp(stretchedLen, size * kStretchMinLenMul, size * kStretchMaxLenMul);
		// 幅(quadWidthLen)は今回のテストでは変えない。
		// 速いほど鋭く見せたい場合はここをscreenSpeedに応じて狭める
	}
	// screenSpeedが閾値以下の場合はquadRightAxis/quadUpAxis/quadWidthLen/quadHeightLenが
	// 初期値(通常の正方形ビルボード)のままなので、何もしなくてよい
#endif

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
