#pragma once

#include <memory>
#include <string>

class KdTexture;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// テクスチャ取得を抽象化するインターフェース
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// EffectInstanceがテクスチャの実際のロード方法を知らずに済むよう、この薄いインターフェース
// 越しにテクスチャを取得する。
// EffectEditor(プレビュー)・EffectDispatcher(実行時)のどちらもテクスチャ取得は
// KdAssets経由で行える為、実装はKdAssetsTextureProvider 1つに一本化する
// (EffectDispatcher側だけ別のリソース管理を新たに用意する必要は無い)。
// インターフェースとして分離してあるのは、テスト用のダミー実装に差し替えられるようにする為、
// および将来テクスチャ取得経路が変わった場合にEffectInstance側を変更せずに済ませる為。
// (これまでEffectEditorはテクスチャを実際に読み込むのに、EffectDispatcherは未実装のまま、
//  という食い違いが起きていた。ここを一本化することで同じ問題の再発を防ぐ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class ITextureProvider
{
public:
	virtual ~ITextureProvider() = default;

	// pathからテクスチャを取得する。
	// pathが空、またはロードに失敗した場合はnullptrを返してよい
	// (EffectInstance側はnullptr＝テクスチャ無しとして扱い、そのまま描画する)
	virtual std::shared_ptr<KdTexture> GetTexture(const std::string& path) = 0;
};
