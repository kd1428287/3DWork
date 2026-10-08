#include "GroundRenderComponent.h"

GroundRenderComponent::GroundRenderComponent(GameObject* owner)
	: ComponentBase(owner)
{
	material_.m_baseColorTex = std::make_shared<KdTexture>();
	material_.m_baseColorTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/Game/Ground/gravel_floor_02_4k.blend/textures/gravel_floor_02_diff_4k.png");
	material_.m_normalTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/Game/Ground/gravel_floor_02_4k.blend/textures/gravel_floor_02_nor_gl_4k.png");
	material_.m_metallicRoughnessTex = KdAssets::Instance().m_textures.GetData("Asset/Textures/Game/Ground/gravel_floor_02_4k.blend/textures/gravel_floor_02_mr_4k.png");
}

void GroundRenderComponent::DrawLit()
{
	// カメラ位置は
	// 
	// 
	// 
	// 
	// 
	// 
	// 
	// のカメラ定数バッファから取得
	const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	KdShaderManager::Instance().m_StandardShader.DrawGround(material_, camPos, halfSize_, tileSize_);
}
