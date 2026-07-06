#pragma once

// ============================================================
// 描画パスを持つコンポーネントだけが実装するインターフェース。
//
// ComponentBaseには入れない。理由:
//   - Movement/Input/CardSelectionのような描画と無関係な
//     コンポーネントにまで描画フックを継承させたくない(ISP違反)
//   - GameObject側が「実際に描画するコンポーネントだけ」を
//     別リストで管理できるようにし、Drawパスの走査コストを
//     描画非対応コンポーネント分だけ削減できる
//
// 使い方: 描画したいコンポーネントは
//   class MeshRenderComponent : public ComponentBase, public IRenderable
// のように多重継承し、必要なパスだけoverrideする。
// 各メソッドは空実装がデフォルトなので、使わないパスは書かなくてよい。
// ============================================================
class IRenderable {
public:
	virtual ~IRenderable() = default;

	virtual void PreDraw() {};
	// 光を遮るオブジェクト(影を落とす側)としてシャドウマップに描く
	virtual void GenerateDepthMapFromLight() {}
	// 陰影のないオブジェクト(背景など)
	virtual void DrawUnLit() {}
	// 陰影のあるオブジェクト(光源の影響を受ける)
	virtual void DrawLit() {}
	// エフェクト(陰影なし)
	virtual void DrawEffect() {}
	// 自ら光るオブジェクト・ブルーム対象
	virtual void DrawBright() {}
	// 2Dスプライト
	virtual void DrawSprite() {}
	// デバッグ描画
	virtual void DrawDebug() {}
};