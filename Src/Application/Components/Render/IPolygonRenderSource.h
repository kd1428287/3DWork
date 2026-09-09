#pragma once

// ============================================================
// KdPolygon(およびその派生: KdTrailPolygon等)の描画データを持つ
// コンポーネントが実装するタグインターフェース。
//
// TAG_INTERFACES(TagInterfaces.h)に登録することで、GameObjectの
// タグレジストリを通じて「同じGameObject上でこれを実装している
// コンポーネント」をGetTagged<IPolygonRenderSource>()で安全に
// 収集できるようになる(登録/解除はAddComponent/RemoveComponent側で
// 自動的に行われるため、実装側は登録処理を書く必要が無い)。
//
// ICameraTarget.hの使い分けコメントにある通り、この仕組みは
// 「同一GameObject内の役割を探す」用途向け。カメラの追従先探しの
// ような「シーン全体から特定の1体を探す」用途とは目的が異なる。
//
// モデル側(将来のIModelRenderSource相当)とはあえて別インターフェース
// にしている。モデルとポリゴンでは「描画に何が必要か」(ワールド行列を
// 使うか、頂点に既に絶対座標が焼き込まれているか等)が異なり、無理に
// 1つのインターフェースへまとめると片方が使わない引数を抱える
// 不自然な共通化になってしまうため。
// ============================================================
class IPolygonRenderSource {
public:
	virtual ~IPolygonRenderSource() = default;

	// 描画対象のKdPolygonを返す。座標(頂点)の更新は実装側(データ保持側)の
	// 責務とし、PolygonRenderComponentは受け取ったものをそのまま描画するだけ。
	virtual KdPolygon* GetPolygon() = 0;

	// 「今描画すべき状態か」。
	// 例: KdTrailPolygonの頂点が2未満(未生成)の間はfalseを返す等。
	virtual bool IsPolygonDrawable() const { return true; }
};
