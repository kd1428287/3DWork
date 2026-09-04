#include "KdTrailPolygon.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// リングバッファアクセス
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
KdTrailPolygon::TrailPoint& KdTrailPolygon::PointAt(size_t indexFromNewest)
{
	const size_t capacity = m_points.size();
	const size_t idx = (m_head + capacity * 2 - 1 - indexFromNewest) % capacity;
	return m_points[idx];
}

const KdTrailPolygon::TrailPoint& KdTrailPolygon::PointAt(size_t indexFromNewest) const
{
	const size_t capacity = m_points.size();
	const size_t idx = (m_head + capacity * 2 - 1 - indexFromNewest) % capacity;
	return m_points[idx];
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 新規ポイントの頂点/UVを計算してキャッシュする(AddPoint時に一度だけ実行)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::CachePointVertices(TrailPoint& point)
{
	Math::Vector3 axisX = point.widthVector;
	float halfWidth = axisX.Length() * 0.5f;

	// 幅ベクトルがほぼ0の場合はNormalizeが不定になるためガードする
	if (halfWidth > 1e-6f)
	{
		axisX.Normalize();
	}
	else
	{
		axisX = { 1.0f, 0.0f, 0.0f };
	}

	// --- eDefault用 ---
	point.vertexA.pos = point.center + axisX * halfWidth;
	point.vertexB.pos = point.center - axisX * halfWidth;
	point.vertexA.UV = { 0.0f, point.uv };
	point.vertexB.UV = { 1.0f, point.uv };

	// --- eVertices用 ---
	point.vertexSingle.pos = point.center;
	point.vertexSingle.UV.x = (float)(m_pointSerial % 2);
	point.vertexSingle.UV.y = point.uv;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ポイントを追加(O(1)償却。頂点計算は新規分のみ)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::AddPoint(const Math::Vector3& center, const Math::Vector3& widthVector)
{
	TrailPoint& point = m_points[m_head];
	point.center = center;
	point.widthVector = widthVector;

	m_nextUV -= m_uvStep; // 新しい点ほど小さい値になるよう単調に変化させる(Wrapサンプラー前提)
	point.uv = m_nextUV;

	CachePointVertices(point);
	++m_pointSerial;

	m_head = (m_head + 1) % m_points.size();
	if (m_count < m_points.size())
	{
		++m_count;
	}
	// m_count == capacityの場合、次回書き込みで最古のポイントが自然に上書きされる

	RebuildVertexArray();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 最後尾(最古)のポイントを削除
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::DelPointBack()
{
	if (m_count == 0) { return; }
	--m_count;

	RebuildVertexArray();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 軌跡ポイントを全て削除
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::ClearPoints()
{
	// m_verticesはここではクリアしない。IsPolygonDrawable()がGetNumPoints()==0を見て
	// 描画をスキップするため実害はなく、次のAddPoint時にRebuildVertexArray()が呼ばれる。
	m_count = 0;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// パターンを設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::SetPattern(Trail_Pattern pattern)
{
	if (m_pattern == pattern) { return; }
	m_pattern = pattern;

	RebuildVertexArray();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 長さ(保持するポイント数の上限)を設定
// 既存ポイントは新しい容量に合わせて古い順に詰め直す(はみ出す分は破棄)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::SetLength(UINT length)
{
	if (length == 0) { length = 1; }
	if (length == m_length && m_points.size() == length) { return; }

	std::vector<TrailPoint> newPoints(length);
	size_t newCount = std::min<size_t>(m_count, length);

	// newPoints[0] = 最古 ... newPoints[newCount-1] = 最新 の順に詰め直す
	for (size_t i = 0; i < newCount; ++i)
	{
		newPoints[i] = PointAt(newCount - 1 - i);
	}

	m_points = std::move(newPoints);
	m_head = newCount % length;
	m_count = newCount;
	m_length = length;

	RebuildVertexArray();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// GPU用頂点配列(m_vertices)を再構築する
// eDefault/eVerticesはキャッシュ済み頂点を並べ直すだけ(再計算なし)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::RebuildVertexArray()
{
	m_vertices.clear();

	switch (m_pattern)
	{
	case Trail_Pattern::eDefault:
	{
		if (m_count < 2) { return; }

		m_vertices.reserve(m_count * 2);
		for (size_t i = 0; i < m_count; ++i)
		{
			const TrailPoint& p = PointAt(i);
			m_vertices.push_back(p.vertexA);
			m_vertices.push_back(p.vertexB);
		}
		break;
	}
	case Trail_Pattern::eVertices:
	{
		if (m_count < 4) { return; }

		m_vertices.reserve(m_count);
		for (size_t i = 0; i < m_count; ++i)
		{
			m_vertices.push_back(PointAt(i).vertexSingle);
		}
		break;
	}
	case Trail_Pattern::eBillboard:
		RebuildBillboardVertices();
		break;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// eBillboardパターン専用の頂点生成
// カメラ向きに依存するため、ポイントが変化していなくても毎回フル計算する必要がある
// (この関数だけはキャッシュの恩恵を受けられない)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdTrailPolygon::RebuildBillboardVertices()
{
	if (m_count < 2) { return; }

	Math::Matrix mCam = KdShaderManager::Instance().GetCameraCB().mView.Invert();

	m_vertices.reserve(m_count * 2);

	for (size_t i = 0; i < m_count; ++i)
	{
		const TrailPoint& p = PointAt(i);

		// ラインの向き
		Math::Vector3 vDir;
		if (i == 0)
		{
			// 初回時のみ、次のポイント(2番目に新しい点)を使用
			vDir = PointAt(1).center - p.center;
		}
		else
		{
			vDir = p.center - PointAt(i - 1).center;
		}

		// カメラからポイントへの向き
		Math::Vector3 v = p.center - mCam.Translation();
		Math::Vector3 axisX = DirectX::XMVector3Cross(vDir, v);

		float halfWidth = p.widthVector.Length() * 0.5f;

		if (axisX.LengthSquared() > 1e-8f)
		{
			axisX.Normalize();
		}
		else
		{
			axisX = { 1.0f, 0.0f, 0.0f };
		}

		Vertex v1, v2;
		v1.pos = p.center + axisX * halfWidth;
		v2.pos = p.center - axisX * halfWidth;
		v1.UV = { 0.0f, p.uv };
		v2.UV = { 1.0f, p.uv };

		m_vertices.push_back(v1);
		m_vertices.push_back(v2);
	}
}