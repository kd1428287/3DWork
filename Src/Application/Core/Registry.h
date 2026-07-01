#pragma once
#include <unordered_map>
#include <memory>
#include <cassert>

//=============================================================================
// Registry
//
// ECSの中核クラス。EntityManagerとコンポーネントストレージを束ねる。
//
// 【依存関係】
//  Registry → EntityManager（メンバとして保持・委譲）
//  Registry → ComponentBase（コンポーネントストレージの基底型として使用）
//
// 【責務】
//  1. EntityManagerへの委譲によるEntityの生成・破棄・有効性確認
//  2. Entityへのコンポーネントの追加・取得・削除
//  3. 指定した型のコンポーネントを持つ全Entityの列挙（View）
//
// 【コンポーネントストレージの構造】
//  m_componentStores[TypeID][EntityIndex] = unique_ptr<ComponentBase>
//  TypeIDはComponentTypeID::Get<T>()で払い出される連番。
//=============================================================================
class Registry
{
public:

	//-------------------------------------------------------------------------
	// Entity操作（EntityManagerへの委譲）
	//-------------------------------------------------------------------------

	Entity CreateEntity() { return m_entityManager.Create(); }
	void   DestroyEntity(Entity entity)
	{
		// 全ストアから該当Entityのコンポーネントを削除してから破棄
		for (auto& [typeID, store] : m_componentStores)
		{
			store.erase(entity.GetIndex());
		}
		m_entityManager.Destroy(entity);
	}
	bool   IsAlive(Entity entity) const { return m_entityManager.IsAlive(entity); }


	//-------------------------------------------------------------------------
	// コンポーネント操作
	//-------------------------------------------------------------------------

	// 追加して参照を返す。追加直後にそのまま設定を続けて書ける。
	// 例: registry.Emplace<CameraComponent>(e).isMainCamera = true;
	template<class T, class... Args>
	T& Emplace(Entity entity, Args&&... args)
	{
		static_assert(std::is_base_of<ComponentBase, T>::value,
			"T must derive from ComponentBase");
		assert(IsAlive(entity) && "Emplace: 無効なEntity");

		size_t typeID = ComponentTypeID::Get<T>();
		EnsureStoreExists(typeID);

		auto comp = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = comp.get();
		m_componentStores[typeID][entity.GetIndex()] = std::move(comp);
		return *raw;
	}

	// 取得（存在しない場合はnullptr）
	template<class T>
	T* TryGet(Entity entity) const
	{
		size_t typeID = ComponentTypeID::Get<T>();
		auto storeIt = m_componentStores.find(typeID);
		if (storeIt == m_componentStores.end()) { return nullptr; }

		auto compIt = storeIt->second.find(entity.GetIndex());
		if (compIt == storeIt->second.end()) { return nullptr; }

		return static_cast<T*>(compIt->second.get());
	}

	// 取得（存在保証がある場合・なければassert）
	template<class T>
	T& Get(Entity entity) const
	{
		T* comp = TryGet<T>(entity);
		assert(comp && "Get: コンポーネントが存在しない");
		return *comp;
	}

	// 存在確認
	template<class T>
	bool Has(Entity entity) const { return TryGet<T>(entity) != nullptr; }

	// 削除
	template<class T>
	void Remove(Entity entity)
	{
		size_t typeID = ComponentTypeID::Get<T>();
		auto storeIt = m_componentStores.find(typeID);
		if (storeIt == m_componentStores.end()) { return; }
		storeIt->second.erase(entity.GetIndex());
	}


	//-------------------------------------------------------------------------
	// View（クエリ）
	//
	// 指定した型を全部持つEntityとコンポーネントの組を列挙する。
	// Systemはこれで処理対象のEntityを受け取る。
	//
	// 例:
	//   registry.View<TransformComponent, CameraComponent>(
	//       [](Entity e, TransformComponent& tr, CameraComponent& cam) { ... });
	//-------------------------------------------------------------------------

	template<class T, class Func>
	void View(Func&& func)
	{
		size_t typeID = ComponentTypeID::Get<T>();
		auto storeIt = m_componentStores.find(typeID);
		if (storeIt == m_componentStores.end()) { return; }

		for (auto& [index, compPtr] : storeIt->second)
		{
			Entity entity = m_entityManager.ReconstructEntity(index);
			if (!entity.IsValid()) { continue; }
			func(entity, static_cast<T&>(*compPtr));
		}
	}

	template<class T1, class T2, class Func>
	void View(Func&& func)
	{
		size_t typeID = ComponentTypeID::Get<T1>();
		auto storeIt = m_componentStores.find(typeID);
		if (storeIt == m_componentStores.end()) { return; }

		for (auto& [index, compPtr] : storeIt->second)
		{
			Entity entity = m_entityManager.ReconstructEntity(index);
			if (!entity.IsValid()) { continue; }

			T2* comp2 = TryGet<T2>(entity);
			if (!comp2) { continue; }

			func(entity, static_cast<T1&>(*compPtr), *comp2);
		}
	}

	template<class T1, class T2, class T3, class Func>
	void View(Func&& func)
	{
		size_t typeID = ComponentTypeID::Get<T1>();
		auto storeIt = m_componentStores.find(typeID);
		if (storeIt == m_componentStores.end()) { return; }

		for (auto& [index, compPtr] : storeIt->second)
		{
			Entity entity = m_entityManager.ReconstructEntity(index);
			if (!entity.IsValid()) { continue; }

			T2* comp2 = TryGet<T2>(entity);
			if (!comp2) { continue; }

			T3* comp3 = TryGet<T3>(entity);
			if (!comp3) { continue; }

			func(entity, static_cast<T1&>(*compPtr), *comp2, *comp3);
		}
	}


private:
	void EnsureStoreExists(size_t typeID)
	{
		if (m_componentStores.find(typeID) == m_componentStores.end())
		{
			m_componentStores.emplace(typeID, ComponentStore{});
		}
	}

	using ComponentStore = std::unordered_map<EntityIndex, std::unique_ptr<ComponentBase>>;

	std::unordered_map<size_t, ComponentStore> m_componentStores;

	// EntityManagerをメンバとして保持する。
	// Registryが唯一の所有者であり、外部から直接触らせない。
	EntityManager m_entityManager;
};