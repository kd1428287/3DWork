#include "../inc_KdCommon.hlsli"
#include "inc_KdGPUParticle.hlsli"	// VSOutputを共用する為(PSもKdGPUParticle_PS.hlslをそのまま流用する)

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)用の頂点入力
//	KdSlashTrailRenderer::Vertex(C++側)とレイアウトを一致させる事
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct VSInput
{
	float3 Position : POSITION;
	float2 UV : TEXCOORD0;
	float4 Color : COLOR;
};

//================================
// 頂点シェーダ：GPUパーティクルと違い、頂点(ワールド座標・UV・色)は
// CPU側(SlashTrailInstance::RebuildVertices())で毎フレーム計算済みのものが
// 渡されてくるだけなので、ここではView×Projで変換するだけで良い
//================================
VSOutput main(VSInput In)
{
	VSOutput Out = (VSOutput) 0;

	Out.Pos = mul(float4(In.Position, 1), g_mView);
	Out.Pos = mul(Out.Pos, g_mProj);

	Out.UV = In.UV;
	Out.Color = In.Color;

	// フェードは頂点カラーのアルファ(In.Color.a)に既に焼き込まれている
	// (SlashTrailInstance側でAgeから計算済み)為、PS側の追加フェード計算
	// (KdGPUParticle_PS.hlslのoutColor.a *= In.LifeRate;)を無効化する目的で1.0固定にする。
	// ここを0〜1の可変値にしてしまうと、Color.aとの二重乗算でフェードが不自然に急になる
	Out.LifeRate = 1.0f;

	return Out;
}
