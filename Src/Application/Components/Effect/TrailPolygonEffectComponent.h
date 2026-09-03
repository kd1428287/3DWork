#pragma once
#include "../Transform/TransformComponent.h"
#include "../Tags/IPolygonRenderSource.h"

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

		Math::Matrix mWorld = transform_->GetWorldMatrix();

		// 3. ワールド座標系に変換
		Math::Vector3 worldRoot = Math::Vector3::Transform(base_, mWorld);
		Math::Vector3 worldTip = Math::Vector3::Transform(tip_, mWorld);

		Math::Vector3 center = (worldTip + worldRoot) * 0.5f;

		// ② 根元から先端へのベクトル (これが帯の幅や長手方向の基準になる)
		Math::Vector3 dir = worldTip - worldRoot;
		float length = dir.Length(); // これを幅として使う場合
		dir.Normalize();

		// ③ 行列の各軸を組み立てる
		// KdTrailPolygon の eDefault パターンでは mat.Right() の長さが width になるため、
		// X軸（Right）に幅の情報を乗せたベクトルを設定します。
		Math::Vector3 axisX = dir * length; // 刃の長さ（幅）をX軸のスケール代わりにする

		// 残りの軸（Y, Z）は適当な直交ベクトル、もしくはカメラ方向・進行方向などを設定
		// ここでは簡易的にワールドのY軸やZ軸を元に直交化するか、
		// DirectX::SimpleMath の機能を使って向きミスのないように回転を作ります。

		// 例：簡易的なマトリックス作成
		Math::Matrix matPoint = Math::Matrix::Identity;

		// Right軸に幅（先端-根元ベクトル）を設定
		matPoint.Right(axisX); // または matPoint._11 = axisX.x; ... 等

		// ※DirectX::SimpleMath::Matrix の場合は以下のように直接ベクトルを代入できます
		// （環境に合わせてメンバ名や関数名は読み替えてください）

		// 確実な行列を作るための構築アプローチ例：
		Math::Vector3 axisY = { 0.0f, 1.0f, 0.0f }; // 上方向など
		Math::Vector3 axisZ = DirectX::XMVector3Cross(axisX, axisY);
		axisZ.Normalize();
		axisY = DirectX::XMVector3Cross(axisZ, axisX);
		axisY.Normalize();

		// マトリックスに反映
		matPoint.Translation(center);
		// スケールや向きを調整した軸をセット
		// （eDefaultパターンでは Right() の長さが width になるため、axisX の長さに注意）

		// ※ KdTrailPolygon::CreateVerticesWithDefaultPattern() の実装を見ると：
		// float width = axisX.Length() * 0.5f; 
		// となっているため、axisX の長さそのものが帯の幅に直結します。

		// したがって、
		// ・axisX = (先端 - 根元) そのものにすれば、刃の長さがそのまま帯の幅（または厚み）になります。
		// ・もし「刃の厚み方向」を幅にしたい場合は、axisX に厚み方向のベクトルを入れてください。

		// シンプルに「先端と根元をそのままaxisX（幅方向）として渡す」場合の書き方例：
		Math::Vector3 bladeVector = worldTip - worldRoot; // 長さと向き

		// 行列の構築
		Math::Matrix finalMat = Math::Matrix::Identity;
		finalMat.Translation(center);

		// Rightベクトルに bladeVector を設定することで、
		// KdTrailPolygon内で幅として計算されます
		// （※SimpleMathのMatrixに直接代入できるメソッドがある場合）
		finalMat.Right(bladeVector);

		m_trailPolygon->AddPoint(finalMat);
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

	void SetBaseTip(Math::Vector3 base, Math::Vector3 tip) { base_ = base; tip_ = tip; }
	void SetPattern(KdTrailPolygon::Trail_Pattern pattern) { m_trailPolygon->SetPattern(pattern); }
	void SetLength(UINT length) { m_trailPolygon->SetLength(length); }
	void ClearTrail() { m_trailPolygon->ClearPoints(); }
	int  GetNumPoints() const { return m_trailPolygon->GetNumPoints(); }

private:
	std::unique_ptr<KdTrailPolygon> m_trailPolygon;

	TransformComponent* transform_ = nullptr;

	Math::Vector3 base_{};
	Math::Vector3 tip_{};
	bool emitting_ = false;
};