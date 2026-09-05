#include "../main.h"
#include "KdAssetsTextureProvider.h"

static const std::string kTextureAssetRoot = "Asset/Textures/Game/Effect/";

std::shared_ptr<KdTexture> KdAssetsTextureProvider::GetTexture(const std::string& path)
{
	if (path.empty()) { return nullptr; }

	return KdAssets::Instance().m_textures.GetData(kTextureAssetRoot + path);
}