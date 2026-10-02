#include "GroundRenderComponent.h"

GroundRenderComponent::GroundRenderComponent(GameObject* owner)
	: ComponentBase(owner)
{
	material_.m_baseColorTex = std::make_shared<KdTexture>();
	material_.m_baseColorTex->Load("Asset/Textures/Ground/albedo.png");
}

void GroundRenderComponent::DrawLit()
{
	// カメラ位置はKdShaderManagerのカメラ定数バッファから取得
	const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	KdShaderManager::Instance().m_StandardShader.DrawGround(material_, camPos, halfSize_, tileSize_);
}
