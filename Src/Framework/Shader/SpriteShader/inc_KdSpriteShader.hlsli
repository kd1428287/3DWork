// 定数バッファ
cbuffer cbSprite : register(b0)
{
    row_major float4x4  g_mTransform;    // 変換行列
    float4              g_color;
    float               g_fillRatio;    // ゲージ用切り抜き比率(0.0～1.0 / 1.0で切り抜きなし)
    float3              g_fillRatioPad; // 16バイト境界合わせ用パディング(未使用)
};

cbuffer cbProjection : register(b1)
{
    row_major float4x4  g_mProj;    // 射影変換行列
};

// 頂点シェーダ出力用
struct VSOutput
{
	float4 Pos : SV_Position;
	float2 UV  : TEXCOORD0;
};
