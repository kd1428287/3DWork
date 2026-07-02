//#pragma once
//#include <vector>
//#include <queue>
//#include <cassert>
//#include "Entity.h"
//
////=============================================================================
//// EntityManager
////
//// Entityの生成・破棄・有効性検証を担う。
//// Registryから分離することで役割を明確にする。
////
//// 【責務の境界】
////  EntityManager → Entityの生存管理（生成・破棄・世代管理）のみ
////  Registry      → コンポーネントストレージの管理
////
////  RegistryがEntityManagerをメンバとして保持し、
////  Entity操作の委譲先として使う。
////
//// 【世代管理の仕組み】
////  生成時: 再利用キュー(m_freeIndices)から取り出すか、新規インデックスを採番する。
////  破棄時: そのインデックスの世代(Gen)を+1してキューに戻す。
////          古い世代のEntityを参照していたコードは IsAlive() で検出できる。
////=============================================================================
//class EntityManager
//{
//public:
//	// 新しいEntityを生成して返す
//	Entity Create()
//	{
//		if (!m_freeIndices.empty())
//		{
//			EntityIndex index = m_freeIndices.front();
//			m_freeIndices.pop();
//			return Entity(index, m_generations[index]);
//		}
//
//		EntityIndex index = static_cast<EntityIndex>(m_generations.size());
//		m_generations.push_back(0);
//		return Entity(index, 0);
//	}
//
//	// Entityを破棄する（世代をインクリメントしてインデックスを再利用可能にする）
//	void Destroy(Entity entity)
//	{
//		assert(IsAlive(entity) && "Destroy: 無効なEntityを破棄しようとした");
//		m_generations[entity.GetIndex()]++;
//		m_freeIndices.push(entity.GetIndex());
//	}
//
//	// Entityが現在有効かどうかを返す（世代の一致で判定）
//	bool IsAlive(Entity entity) const
//	{
//		EntityIndex index = entity.GetIndex();
//		if (index >= m_generations.size()) { return false; }
//		return m_generations[index] == entity.GetGen();
//	}
//
//	// インデックスから現在有効なEntityを再構築する（Registry::View内で使用）
//	Entity ReconstructEntity(EntityIndex index) const
//	{
//		if (index >= m_generations.size()) { return Entity{}; }
//		Entity e(index, m_generations[index]);
//		if (!IsAlive(e)) { return Entity{}; }
//		return e;
//	}
//
//private:
//	// インデックスごとの現在の世代カウンタ
//	std::vector<EntityGen> m_generations;
//
//	// 再利用可能なインデックスのキュー
//	std::queue<EntityIndex> m_freeIndices;
//};