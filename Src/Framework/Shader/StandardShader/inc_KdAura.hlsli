// オーラ用VSからPSへ渡す値
struct VSOutputAura
{
	float4 Pos : SV_Position;
	float3 wPos : TEXCOORD0;
	float3 wN : TEXCOORD1;
};
