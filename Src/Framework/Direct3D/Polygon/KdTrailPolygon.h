#pragma once

class KdTrailPolygon : public KdPolygon
{
public:
	enum class Trail_Pattern
	{
		eDefault,
		eBillboard,
		eVertices
	};

	KdTrailPolygon() { m_points.resize(m_length); }
	KdTrailPolygon(const std::shared_ptr<KdTexture>& spBaseColTex) : KdPolygon(spBaseColTex) { m_points.resize(m_length); }
	KdTrailPolygon(const std::string& baseColTexName) : KdPolygon(baseColTexName) { m_points.resize(m_length); }

	// ポイントを追加
	// center      : 帯の中心線上の点(ワールド座標)
	// widthVector : 帯の幅方向×全幅を表すベクトル(eDefault/eBillboardで使用)
	void AddPoint(const Math::Vector3& center, const Math::Vector3& widthVector);

	// 最後尾(最も古い)のポイントを削除
	void DelPointBack();

	// 軌跡ポイントを全て削除
	void ClearPoints();

	// 現在のポイント数を取得
	inline int GetNumPoints() const { return (int)m_count; }

	// パターンを設定
	void SetPattern(Trail_Pattern pattern);

	// 帯状ポリゴンの長さ(保持するポイント数の上限)を設定7
	void SetLength(UINT length);

private:

	// 1ポイント分のデータ。頂点座標とUVは生成時に計算し以後不変。
	struct TrailPoint
	{
		Math::Vector3 center;
		Math::Vector3 widthVector;
		float uv = 0.0f; // 生成時に確定するV座標(Wrapサンプラー前提)

		// eDefault用キャッシュ済み頂点(生成時に計算し以後不変)
		Vertex vertexA; // 幅方向+側
		Vertex vertexB; // 幅方向-側

		// eVertices用キャッシュ済み頂点(生成時に計算し以後不変)
		Vertex vertexSingle;
	};

	// リングバッファアクセス(indexFromNewest = 0が最新、count-1が最古)
	TrailPoint& PointAt(size_t indexFromNewest);
	const TrailPoint& PointAt(size_t indexFromNewest) const;

	// 新規ポイントの頂点/UVを計算してキャッシュする
	void CachePointVertices(TrailPoint& point);

	// m_pattern に応じてGPU用頂点配列(m_vertices)を再構築する
	void RebuildVertexArray();

	// eBillboard専用: カメラ依存のため毎回フル計算が必要
	void RebuildBillboardVertices();

	// ポリゴンの生成パターン
	Trail_Pattern m_pattern = Trail_Pattern::eDefault;

	// 軌跡のポイント(リングバッファ、capacity = m_length)
	std::vector<TrailPoint> m_points;
	size_t m_head = 0;   // 次に書き込む位置(インデックス)
	size_t m_count = 0;  // 現在の有効ポイント数

	// UV.y割り当て用。ポイント生成順に単調に変化させ、以後不変にする。
	float m_nextUV = 0.0f;
	float m_uvStep = 0.05f;

	// 生成順の通し番号(eVerticesパターンのUV.xの0/1切り替えに使用。
	// ポイントの寿命中は不変)
	unsigned int m_pointSerial = 0;

	// 軌跡の長さ(保持するポイント数の上限)
	UINT m_length = 40;
};