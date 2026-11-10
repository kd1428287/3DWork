#include "../inc_KdCommon.hlsli"
#include "../GPUParticle/inc_GPUParticleShader.hlsli"	// VSOutputを共用する為

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)用の頂点入力
//	KdSlashTrailRenderer::Vertex(C++側)とレイアウトを一致させる事
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct VSInput
{
	float3 Position : POSITION;
	float2 UV : TEXCOORD0;
	float4 Color : COLOR;
	float4 Extra : TEXCOORD1;	// x=消去用LifeRate(1→0) yz=ノイズUV w=予約
};

//================================
// 頂点シェーダ：頂点(ワールド座標・UV・色)はCPU側(SlashTrailInstance::RebuildVertices())で
// 毎フレーム計算済みなので、ここではView×Projで変換するだけ
//================================
VSOutput main(VSInput In)
{
	VSOutput Out = (VSOutput) 0;

	Out.Pos = mul(float4(In.Position, 1), g_mView);
	Out.Pos = mul(Out.Pos, g_mProj);

	Out.UV = In.UV;
	Out.Color = In.Color;

	// 消去はパーティクルと同じ仕組み：LifeRateをPS側のノイズ(ディゾルブ)が使う。
	// ノイズ無しの場合も、PS側がLifeRateで線形フェードする
	Out.LifeRate = In.Extra.x;

	// ノイズUVはSeedで渡す(PS側はSeedを足さずにそのままノイズUVとして使う)
	Out.Seed = In.Extra.yz;

	return Out;
}
