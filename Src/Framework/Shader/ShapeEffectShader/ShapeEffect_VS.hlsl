// カメラ(KdShaderManager::cbCamera と一致させる事)
cbuffer cbCamera : register(b7)
{
	row_major float4x4	g_mView;
	row_major float4x4	g_mProj;
	row_major float4x4	g_mProjInv;
	float3				g_CamPos;
	int					g_blank;
};

// ShapeEffectShader::cbShape と一致させる事
cbuffer cbShapeEffect : register(b5)
{
	row_major float4x4	g_mWorld;
	float4				g_Color;
	float				g_DepthOffset;	// ビュー空間でカメラ方向へずらす量(+で手前)
	float3				g_pad;
};

struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV  : TEXCOORD0;
};

// 4頂点のトライアングルストリップで、ローカルXY平面の1x1クアッドを作る
VSOutput main(uint id : SV_VertexID)
{
	const float2 corner = float2((id & 1) ? 0.5f : -0.5f, (id & 2) ? -0.5f : 0.5f);

	float4 wpos = mul(float4(corner, 0.0f, 1.0f), g_mWorld);
	float4 viewPos = mul(wpos, g_mView);
	float4 clip = mul(viewPos, g_mProj);

	// 深度だけをずらす(画面上の位置・大きさ=clip.xywは変えず、z/wだけをずらした点の値にする)
	if (g_DepthOffset != 0.0f)
	{
		float4 shifted = mul(float4(viewPos.x, viewPos.y, viewPos.z - g_DepthOffset, 1.0f), g_mProj);
		if (shifted.w > 0.0001f) { clip.z = shifted.z * (clip.w / shifted.w); }
	}

	VSOutput Out;
	Out.Pos = clip;
	Out.UV = float2((float)(id & 1), (float)((id >> 1) & 1));
	return Out;
}
