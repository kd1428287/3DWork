#pragma once

#include "ITextureProvider.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// KdAssets経由でテクスチャを取得するITextureProviderの実装
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EffectEditor(プレビュー)・EffectDispatcher(実行時)のどちらもテクスチャ取得はKdAssets経由で
// 行える為、実装はこの1クラスに一本化する。EffectDispatcher側だけ別のリソース管理を
// 新たに実装する必要は無い(以前のTODOコメントはこれで解消できる)。
//
// テクスチャアセットのルートパス(Asset/Textures/Game/Effect/)はこのクラス内に閉じ込めてある。
// ※EffectEditor.cpp側に同名のkTextureAssetRoot(テクスチャピッカーのファイル走査用)が
//   別途あるが、これは「候補一覧をディスクから探す」為のものでこのクラスとは役割が異なる為、
//   重複ではあるが両方に置いたままにしている。将来的に一本化する場合はここを共通定数化すること。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdAssetsTextureProvider : public ITextureProvider
{
public:

	// path(テクスチャアセットのルートからの相対パス)からKdAssets経由でテクスチャを取得する。
	// pathが空、またはKdAssets側で見つからない場合はnullptrを返す。
	std::shared_ptr<KdTexture> GetTexture(const std::string& path) override;
};