#include "inc_KdStandardShader.hlsli"
#include "../inc_KdCommon.hlsli"
#include "inc_KdMotionVector.hlsli"

// 速度バッファ用の前フレーム情報(C++側のcbMotion。スキンメッシュのLit描画時のみEnable=1)
cbuffer cbMotion : register(b5)
{
	row_major float4x4 g_mPrevViewProj;
	row_major float4x4 g_mPrevWorld;
	int g_MotionEnable;
	float3 _motionPad;
};

// 前フレームのボーン行列(C++側のcbPrevBone)
cbuffer cbPrevBone : register(b6)
{
	row_major float4x4 g_mPrevBones[300];
};

//================================
// 頂点シェーダ
//================================
VSOutputLit main(
	float4 pos : POSITION,			// 頂点座標
	float2 uv : TEXCOORD0,			// テクスチャUV座標
	float4 color : COLOR,			// 頂点カラー
	float3 normal : NORMAL,			// 法線
	float3 tangent : TANGENT,		// 接線
    uint4 skinIndex : SKININDEX,	// スキンメッシュのボーンインデックス(何番目のボーンに影響しているか？のデータ(最大４つぶん))
	float4 skinWeight : SKINWEIGHT	// ボーンの影響度(そのボーンにどれだけ影響しているか？のデータ(最大４つぶん))
)
{
	// 前フレーム位置の計算用に、スキニング前の頂点を保存
	float4 localPos = pos;

	// スキニング---------------->
	if (g_IsSkinMeshObj)
	{
		// 行列を合成
		row_major float4x4 mBones = 0; // 行列を0埋め
		[unroll]
		for (int i = 0; i < 4; i++)
		{
			mBones += g_mBones[skinIndex[i]] * skinWeight[i];
		}

		// 座標と法線に適用
		pos = mul(pos, mBones);
		normal = mul(normal, (float3x3) mBones);
	}
	// <----------------スキニング
	
	VSOutput Out;

    // 座標変換
	Out.Pos = mul(pos, g_mWorld);	 // ローカル座標系	-> ワールド座標系へ変換
	// 輪郭パス：ワールド法線方向へ押し出す(スケールに依存しない太さ)
	if (g_OutlineWidth > 0)
	{
		Out.Pos.xyz += normalize(mul(normal, (float3x3) g_mWorld)) * g_OutlineWidth;
	}

	Out.wPos = Out.Pos.xyz;			 // ワールド座標を別途保存
	Out.Pos = mul(Out.Pos, g_mView); // ワールド座標系	-> ビュー座標系へ変換
	Out.Pos = mul(Out.Pos, g_mProj); // ビュー座標系	-> 射影座標系へ変換

    // 頂点色
	Out.Color = color;

    // 法線
	Out.wN = normalize(mul(normal, (float3x3) g_mWorld));
    // 接線
	Out.wT = normalize(mul(tangent, (float3x3) g_mWorld));
    // 従接線
	float3 binormal = cross(normal, tangent);
	Out.wB = normalize(mul(binormal, (float3x3) g_mWorld));

    // UV座標
	Out.UV = uv * g_UVTiling + g_UVOffset;
	
    // 出力
	// 速度用のクリップ座標(前フレーム側はEnable時のみ計算。無効時はw=0で「速度なし」の印)
	VSOutputLit Res;
	Res.Base = Out;
	Res.CurClip = Out.Pos;
	Res.PrevClip = float4(0, 0, 0, 0);

	if (g_MotionEnable)
	{
		float4 prevPos = localPos;
		if (g_IsSkinMeshObj)
		{
			row_major float4x4 mPrevBones = 0;
			[unroll]
			for (int j = 0; j < 4; j++)
			{
				mPrevBones += g_mPrevBones[skinIndex[j]] * skinWeight[j];
			}
			prevPos = mul(localPos, mPrevBones);
		}
		prevPos = mul(prevPos, g_mPrevWorld);
		Res.PrevClip = mul(prevPos, g_mPrevViewProj);
	}

	return Res;
}
