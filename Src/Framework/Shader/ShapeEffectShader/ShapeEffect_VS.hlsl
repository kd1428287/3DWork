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

	VSOutput Out;
	Out.Pos = mul(mul(wpos, g_mView), g_mProj);
	Out.UV = float2((float)(id & 1), (float)((id >> 1) & 1));
	return Out;
}
