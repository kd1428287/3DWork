#pragma once
#include "../Transform/TransformComponent.h"
#include "../Tags/IPolygonRenderSource.h"
//#include "KdTrailPolygon.h"

// ============================================================
// KdTrailPolygonの「データ管理」を担当するコンポーネント。
// IPolygonRenderSourceを実装することで、同じGameObject上の
// PolygonRenderComponentから自動的に見つけて描画してもらえる
// (GameObjectのタグレジストリ経由。登録/解除はAddComponent/
//  RemoveComponent側で自動的に行われるため、このクラス側は
//  Start()で自分から登録しにいく処理を書く必要すらない)。
//
// PolygonRenderComponentが付いていないGameObjectに付けた場合は、
// 単に描画されない(Update()自体は問題なく動く)だけになる。
// ============================================================
class TrailPolygonComponent : public ComponentBase, public IPolygonRenderSource {
public:
	explicit TrailPolygonComponent(GameObject* owner, const std::string& baseColTexName = "")
		: ComponentBase(owner)
	{
		m_trailPolygon = baseColTexName.empty()
			? std::make_unique<KdTrailPolygon>()
			: std::make_unique<KdTrailPolygon>(baseColTexName);
	}

	void Start() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();
	}

	// 記録中のみ、毎フレーム現在のワールド行列をトレイルへ追加する。
	void Update(float /*deltaTime*/) override
	{
		if (!emitting_ || !transform_) { return; }

		m_trailPolygon->AddPoint(transform_->GetWorldMatrix());
	}

	// ---------------------------------------------------------------
	// IPolygonRenderSource
	// ---------------------------------------------------------------

	KdPolygon* GetPolygon() override { return m_trailPolygon.get(); }

	bool IsPolygonDrawable() const override
	{
		// 頂点が2未満(=ポイントが2つ未満)の場合はGenerateVertices側で
		// 頂点配列が空のままなので、無駄な描画呼び出しを避ける。
		return m_trailPolygon && m_trailPolygon->GetNumPoints() >= 2;
	}

	// ---------------------------------------------------------------
	// 軌跡の記録制御
	// ---------------------------------------------------------------

	// 記録を開始する。前回分の軌跡が残っていると振り始めに古い軌跡が
	// 一瞬繋がって見えるため、開始時にクリアしておく。
	void StartEmit()
	{
		if (emitting_) { return; }

		m_trailPolygon->ClearPoints();
		emitting_ = true;
	}

	// 記録を停止する。既に生成済みの頂点はそのまま残る
	// (最後の軌跡が自然に流れて消えていく見た目を想定)。
	void StopEmit() { emitting_ = false; }

	bool IsEmitting() const { return emitting_; }

	// ---------------------------------------------------------------
	// KdTrailPolygonの設定への委譲
	// ---------------------------------------------------------------

	void SetPattern(KdTrailPolygon::Trail_Pattern pattern) { m_trailPolygon->SetPattern(pattern); }
	void SetLength(UINT length) { m_trailPolygon->SetLength(length); }
	void ClearTrail() { m_trailPolygon->ClearPoints(); }
	int  GetNumPoints() const { return m_trailPolygon->GetNumPoints(); }

private:
	std::unique_ptr<KdTrailPolygon> m_trailPolygon;

	TransformComponent* transform_ = nullptr;

	bool emitting_ = false;
};