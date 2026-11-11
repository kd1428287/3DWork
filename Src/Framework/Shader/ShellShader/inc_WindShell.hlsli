// 風シェルのVS/PS共通定義。C++側WindShellParamsとレイアウトを必ず一致させる事(160バイト)

cbuffer cbWindShell : register(b10)	// ※C++側WindShellShader::kCBSlotと一致させる
{
	row_major float4x4 g_ShellWorld;	// 回転+移動のみ(+Yが軸。スケールは入れない)
	float4 g_Color;

	float g_Length;			// 円筒部の長さ(0で球)
	float g_Radius;			// 半径(両端の半球も同じ半径)
	float g_Taper;			// 後方のすぼまり(0-1)
	float g_Twist;			// 軸方向のねじれ量(ラジアン)

	float g_Time;
	float g_SpinSpeed;		// 回転速度(ラジアン/秒)
	float g_NoiseAmp;		// 頂点ノイズによる半径の揺らぎ
	float g_NoiseFreq;

	float g_StreakCount;	// 周方向の筋の繰り返し数(整数にする事。継ぎ目が出る)
	float g_StreakStretch;	// 軸方向のUV倍率(小さいほど長い筋)
	float g_FlowSpeed;		// 筋が軸方向へ流れる速さ
	float g_Threshold;		// 筋の切り抜きしきい値

	float g_Softness;		// 切り抜きの柔らかさ
	float g_RimPower;		// 輪郭付近を強調する指数
	float g_Alpha;
	float g_Seed;			// レイヤーごとの位相ずらし

	float g_LifeRate;		// 消去用 1→0
	float3 _windPad;
};

struct WindVSOut
{
	float4 Pos : SV_Position;
	float2 StreakUV : TEXCOORD0;
	float3 wPos : TEXCOORD1;
	float3 wN : TEXCOORD2;
	float2 ShellUV : TEXCOORD3;	// 消去ノイズ用(x=角度 y=軸方向)
	float Fade : TEXCOORD4;
};
