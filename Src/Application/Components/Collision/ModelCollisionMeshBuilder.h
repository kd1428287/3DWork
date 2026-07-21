#pragma once

#include "TriangleMeshData.h"
#include "../../Framework/Model/KdModel.h" // KdModelData

// ============================================================
// KdModelData(glTF読み込み済みのモデルデータ)から、
// ColliderComponent::AddMesh() にそのまま渡せる TriangleMeshData を
// 構築するユーティリティ。
//
// 対象ノードは KdModelData::GetCollisionMeshNodeIndices() が返す
// 「名前に"COL"を含む当たり判定専用ノード」(KdModel.cpp::CreateNodes()参照)。
// 当たり判定用ノードが1つも無いモデルの場合は、CreateNodes()側のフォールバック
// (描画ノード = 判定ノードとして扱う)によって自動的に描画メッシュがそのまま
// 対象になる。
//
// 各ノードの頂点は「ノード自身のワールド行列」(モデルのルートノードから見た
// 相対位置。アニメーションはしない前提の、モデル内部の静的な階層分)で変換して
// から1つの配列にまとめる。TriangleMeshDataは
// GameObjectのTransformComponentから見てローカル座標系の原点基準で保持される
// 前提(ColliderComponent.h参照)なので、モデル内部のノード階層分だけここで
// 先に「焼き込んで」おき、実行時はTransformComponentのワールド行列だけを
// 掛ければ良い状態にしておく。
//
// 使用例:
//   auto meshData = KdModelCollisionMeshBuilder::Build(modelData);
//   colliderComponent->AddMesh("terrain", meshData, ColliderLayer::Ground);
//
// ※ TriangleMeshData の実際のメンバ名がここで想定しているもの
//   (positions / triangles / boundsCenter / boundsRadius)と異なる場合は、
//   該当箇所だけ実際の定義に合わせて調整してください。
// ============================================================
namespace KdModelCollisionMeshBuilder
{
	// KdModelDataから、当たり判定専用のTriangleMeshDataを構築する。
	inline std::shared_ptr<TriangleMeshData> Build(const std::shared_ptr<KdModelData>& model)
	{
		std::vector<Math::Vector3> positions;
		std::vector<std::array<uint32_t, 3>> triangles;

		if (!model) { return TriangleMeshData::Create(std::move(positions), std::move(triangles)); }

		const std::vector<int>& targetNodeIndices = model->GetCollisionMeshNodeIndices();
		const std::vector<KdModelData::Node>& nodes = model->GetOriginalNodes();

		for (int nodeIdx : targetNodeIndices)
		{
			if (nodeIdx < 0 || nodeIdx >= (int)nodes.size()) { continue; }

			const KdModelData::Node& node = nodes[nodeIdx];
			if (!node.m_spMesh) { continue; }

			const std::vector<Math::Vector3>& localPositions = node.m_spMesh->GetVertexPositions();
			const std::vector<KdMeshFace>& faces = node.m_spMesh->GetFaces();

			// このノードの頂点が、まとめた配列の中で何番目から始まるか
			// (複数ノードの頂点を1つの配列に連結するためのオフセット)
			const uint32_t baseIndex = (uint32_t)positions.size();

			// 頂点座標: ノード自身のワールド行列(モデルルートから見た相対位置)で変換して追加
			positions.reserve(positions.size() + localPositions.size());
			for (const Math::Vector3& localPos : localPositions)
			{
				positions.push_back(Math::Vector3::Transform(localPos, node.m_worldTransform));
			}

			// 面情報: オフセットを加算しつつ、3点(トライアングル)としてそのまま登録
			triangles.reserve(triangles.size() + faces.size());
			for (const KdMeshFace& face : faces)
			{
				triangles.push_back({
					baseIndex + face.Idx[0],
					baseIndex + face.Idx[1],
					baseIndex + face.Idx[2]
					});
			}
		}

		// 境界球の計算含め、生成はTriangleMeshData::Create()に任せる
		// (RecalculateBounds()を内部で呼んでくれるため、ここで自前計算する必要は無い)
		return TriangleMeshData::Create(std::move(positions), std::move(triangles));
	}
}
