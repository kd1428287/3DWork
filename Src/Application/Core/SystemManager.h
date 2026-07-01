#pragma once
#include <vector>
#include <memory>
#include "Registry.h"

//=============================================================================
// SystemManager
//
// Systemの登録と実行順序の管理を担う。
//
// 【設計方針】
//  ・Systemは登録順に実行される。依存関係は登録順で表現する。
//  ・RegistryはSystemManagerが保持し、全Systemで共有する。
//  ・Systemの追加はAddSystem<T>()で行い、型引数でSystemを指定する。
//
// 使い方:
//   SystemManager manager;
//   manager.AddSystem<CameraRotateSystem>();  // 1番目に実行
//   manager.AddSystem<CameraFollowSystem>();  // 2番目に実行
//   manager.AddSystem<CameraRenderSystem>();  // 3番目に実行
//
//   // メインループ内
//   manager.UpdateAll();
//=============================================================================
class SystemManager
{
public:
	// Systemを登録する（登録順 = 実行順）
	template<class T, class... Args>
	T& AddSystem(Args&&... args)
	{
		static_assert(std::is_base_of<SystemBase, T>::value,
			"T must derive from SystemBase");

		auto system = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = system.get();
		m_systems.push_back(std::move(system));
		return *raw;
	}

	// 全Systemを登録順に実行する
	void UpdateAll()
	{
		for (auto& system : m_systems)
		{
			system->Update(m_registry);
		}
	}

	// Registryへのアクセス（Entity・コンポーネントの登録に使う）
	Registry& GetRegistry() { return m_registry; }

private:
	Registry m_registry;
	std::vector<std::unique_ptr<SystemBase>> m_systems;
};