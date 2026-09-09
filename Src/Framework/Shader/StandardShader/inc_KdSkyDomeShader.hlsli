
cbuffer cbSkyDome : register(b4) // 空いている番号を使う。既存: b0(Obj) b1(Mesh) b2(Material) b3(Bone)
{
	float g_EdgeFadeBottomY;
	float g_EdgeFadeTopY;
	float2 _blank;
};
