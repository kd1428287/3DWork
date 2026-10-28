// Litパス専用のVS出力：既存のVSOutputに、速度算出用のクリップ座標を足したもの
// ※先に inc_KdStandardShader.hlsli をincludeしてから、このファイルをincludeすること

struct VSOutputLit
{
	VSOutput Base;
	float4 CurClip  : TEXCOORD8;	// 現在フレームのクリップ座標
	float4 PrevClip : TEXCOORD9;	// 前フレームのクリップ座標(速度なしの時はw=0)
};
