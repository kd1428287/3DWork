#include "../main.h"
#include "KdAssetsTextureProvider.h"

// テクスチャアセットの走査ルート(実際のプロジェクト構成に合わせて調整。EffectEditor.cppの
// kTextureAssetRootと同一のものを指す)
static const std::string kTextureAssetRoot = "Asset/Texture/";

std::shared_ptr<KdTexture> KdAssetsTextureProvider::GetTexture(const std::string& path)
{
	if (path.empty()) { return nullptr; }

	return KdAssets::Instance().m_textures.GetData(kTextureAssetRoot + path);
}
