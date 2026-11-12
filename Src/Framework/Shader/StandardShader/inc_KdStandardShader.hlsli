// 定数バッファ(オブジェクト単位)
cbuffer cbObject : register(b0)
{
	float2	g_UVOffset;
	float2	g_UVTiling;
	
	int g_FogEnable;	// フォグ有効
	int g_OnlyEmissie;	// エミッシブの描画だけにするかどうか
	int g_IsSkinMeshObj;// スキンメッシュオブジェクトかどうか(スキンメッシュ対応)
	bool g_ForceMaxDepth;
	
	float g_dissolveValue;		// ディゾルブの閾値
	float g_dissolveEdgeRange;	// ディゾルブの境界線の太さ
	float3 g_dissolveEmissive;	// 境界の色
	float g_OutlineWidth;		// 輪郭線(裏面拡大)の太さ。0なら通常描画

	float3 g_InkBlack;			// 墨変換：最暗の墨色
	float  g_InkAmount;			// 0=変換なし 1=完全に墨
	float3 g_InkPaper;			// 墨変換：最明の紙色
	float  g_InkBias;			// 1未満で白寄り、1超で黒寄り
	float  g_InkContrast;
	float  g_InkSteps;			// 濃淡の段数(0で連続)
	float  g_InkLumaScale;
	float  g_InkNoiseAmount;	// 濃淡ゆらぎの強さ(0で無効。白寄りの面ほど効く)
	float  g_InkNoiseScale;		// ゆらぎの細かさ(ワールド座標への倍率)
	float3 _inkPad;

	float3 g_AuraColor;
	float g_AuraIntensity;
	float3 g_AuraCoreColor;
	float g_AuraWidth;
	float g_AuraFresnelPower;
	float g_AuraNoiseScale;
	float g_AuraScrollSpeed;
	float g_AuraTime;
	float g_AuraRise;
	float g_AuraBaseY;
	float g_AuraHeight;
	float g_AuraSoftness;

	int g_AuraKind;
	float3 _auraKindPad;
};

// 定数バッファ(メッシュ単位)
cbuffer cbMesh : register(b1)
{
	// オブジェクト情報
	row_major float4x4 g_mWorld; // ワールド変換行列
};

cbuffer cbMaterial : register(b2)
{
	float4	g_BaseColor; // ベース色
	float3	g_Emissive;  // 自己発光色
	float	g_Metallic;	 // 金属度
	float	g_Roughness; // 粗さ
};

// ボーン行列配列(スキンメッシュ対応)
cbuffer cbBones : register(b3)
{
	row_major float4x4 g_mBones[300];
};

// 頂点シェーダから出力するデータ
struct VSOutput
{
	float4 Pos	 : SV_Position;	// 射影座標
	float3 wPos  : TEXCOORD0;	// ワールド3D座標

	float2 UV	 : TEXCOORD1;	// UV座標
	float4 Color : TEXCOORD2;	// 色

	float3 wN	 : TEXCOORD3;	// ワールド法線
	float3 wT	 : TEXCOORD4;	// ワールド接線
	float3 wB	 : TEXCOORD5;	// ワールド従法線
};

struct VSOutputNoLighting
{
	float4 Pos	 : SV_Position; // 射影座標
	float3 wPos  : TEXCOORD0;	// ワールド3D座標

	float2 UV	 : TEXCOORD1;	// UV座標
	float4 Color : TEXCOORD2;	// 色
};

struct VSOutputGenShadow
{
	float4 Pos	 : SV_Position;	// 射影座標
	float4 pPos  : TEXCOORD0;	// 射影座標（VP変換無し

	float2 UV	 : TEXCOORD1;	// UV座標
	float4 Color : TEXCOORD2;	// 色
};
