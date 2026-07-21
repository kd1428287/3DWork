#pragma once

// ============================================================
// 当たり判定専用の三角形メッシュデータ(見た目の描画は一切持たない)。
//
// KdMesh(頂点バッファ・インデックスバッファを持つ描画用メッシュ)の
// うち、当たり判定に必要な部分(座標配列・面配列・境界データ)だけを
// 抜き出したもの。GPUリソースを一切持たないため、描画を伴わない
// 当たり判定専用の地形メッシュ(読み込んだモデルの描画メッシュとは
// 別に、判定専用の簡略化ジオメトリを用意するケースなど)にも使い回せる。
//
// ColliderComponentのCollisionShapeEntry(shape==Mesh)がshared_ptrで
// 保持する想定(KdModelDataが複数のKdModelWorkから共有されるのと
// 同じ考え方。同じモデルアセットを複数のGameObjectが当たり判定に
// 使う場合、頂点/面配列を複製せずに済む)。
//
// ローカル座標系の境界球をあらかじめ計算しておき、
// CollisionShapeEntryのブロードフェーズ判定に使う(KdMesh::m_bsと
// 同じ役割)。境界AABBは持たない。回転すると軸並行でなくなってしまう
// ため、回転にも崩れない境界球のみに統一している。
// ============================================================
struct TriangleMeshData
{
	// 頂点座標(ローカル座標系)
	std::vector<Math::Vector3> positions;

	// 三角形を構成する頂点インデックス3つ組
	std::vector<std::array<uint32_t, 3>> triangles;

	// ブロードフェーズ用: ローカル座標系での境界球
	Math::Vector3	boundsCenter{};
	float			boundsRadius = 0.0f;

	// positions/trianglesをセットした後に呼び、境界情報を再計算する。
	void RecalculateBounds()
	{
		if (positions.empty()) {
			boundsCenter = Math::Vector3::Zero;
			boundsRadius = 0.0f;
			return;
		}

		Math::Vector3 minP = positions[0];
		Math::Vector3 maxP = positions[0];
		for (const Math::Vector3& p : positions) {
			minP = Math::Vector3::Min(minP, p);
			maxP = Math::Vector3::Max(maxP, p);
		}

		boundsCenter = (maxP + minP) * 0.5f;

		boundsRadius = 0.0f;
		for (const Math::Vector3& p : positions) {
			boundsRadius = std::max(boundsRadius, (p - boundsCenter).Length());
		}
	}

	// 頂点配列 + 3つ組indexの面配列から生成。
	static std::shared_ptr<TriangleMeshData> Create(
		std::vector<Math::Vector3> verts, std::vector<std::array<uint32_t, 3>> tris)
	{
		auto data = std::make_shared<TriangleMeshData>();
		data->positions = std::move(verts);
		data->triangles = std::move(tris);
		data->RecalculateBounds();
		return data;
	}

	// 頂点配列 + フラットなindex配列(3つで1面)から生成する簡易版。
	// モデルローダーなど、フラットなindexバッファしか持っていない
	// データソースから作る場合に使う。
	static std::shared_ptr<TriangleMeshData> CreateFromFlatIndices(
		std::vector<Math::Vector3> verts, const std::vector<uint32_t>& flatIndices)
	{
		std::vector<std::array<uint32_t, 3>> tris;
		tris.reserve(flatIndices.size() / 3);

		for (size_t i = 0; i + 2 < flatIndices.size(); i += 3) {
			tris.push_back({ flatIndices[i], flatIndices[i + 1], flatIndices[i + 2] });
		}

		return Create(std::move(verts), std::move(tris));
	}
};
