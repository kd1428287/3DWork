#pragma once

// ============================================================
// GameObject: コンポーネントの入れ物。
// 型ごとに1インスタンスまで(Unityの基本挙動と同様)を前提とする。
// 複数アタッチしたい場合は GameObject を分けるか、
// このクラスをmulti-component対応に拡張する。
// ============================================================
class GameObject {
public:
	// eventBusはowner_と同様に「非所有の参照」。
	// Sceneが所有するLocalEventBusを、生成時に一度だけ紐付ける。
	explicit GameObject(std::string name = "GameObject", EventBus* eventBus = nullptr)
		: name_(std::move(name)), eventBus_(eventBus) {}

	// GameObjectはポインタ経由で管理される前提のため、コピー禁止
	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;

	~GameObject() { DestroyAllComponents(); }

	// --- コンポーネント操作 ----------------------------------------

	// コンポーネントを生成してアタッチする。Args はコンポーネントの
	// コンストラクタに (GameObject* owner を除いて) そのまま渡される。
	template <typename T, typename... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of_v<ComponentBase, T>,
			"T must derive from ComponentBase");

		const ComponentTypeId id = GetComponentTypeId<T>();
		auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* rawPtr = component.get();

		components_[id] = std::move(component);
		componentOrder_.push_back(id);

		rawPtr->Awake();
		return rawPtr;
	}

	// 指定した型のコンポーネントを取得する。無ければ nullptr。
	template <typename T>
	T* GetComponent() const {
		auto it = components_.find(GetComponentTypeId<T>());
		if (it == components_.end()) {
			return nullptr;
		}
		return static_cast<T*>(it->second.get());
	}

	template <typename T>
	bool HasComponent() const {
		return components_.find(GetComponentTypeId<T>()) != components_.end();
	}

	// 指定した型のコンポーネントを破棄する。
	template <typename T>
	void RemoveComponent() {
		const ComponentTypeId id = GetComponentTypeId<T>();
		auto it = components_.find(id);
		if (it == components_.end()) {
			return;
		}
		it->second->OnDestroy();
		components_.erase(it);
		componentOrder_.erase(
			std::remove(componentOrder_.begin(), componentOrder_.end(), id),
			componentOrder_.end());
	}

	// --- ライフサイクル駆動 ----------------------------------------
	// これらは通常 World / Scene 側のループから呼び出される。

	void Update(float deltaTime) {
		if (!active_) return;
		for (const auto id : componentOrder_) {
			ComponentBase* comp = components_[id].get();
			if (!comp->IsEnabled()) continue;
			if (!comp->started_) {
				comp->Start();
				comp->started_ = true;
			}
			comp->Update(deltaTime);
		}
	}

	void LateUpdate(float deltaTime) {
		if (!active_) return;
		for (const auto id : componentOrder_) {
			ComponentBase* comp = components_[id].get();
			if (comp->IsEnabled()) {
				comp->LateUpdate(deltaTime);
			}
		}
	}

	void DestroyAllComponents() {
		// アタッチと逆順に破棄するのが安全(依存関係を考慮)
		for (auto it = componentOrder_.rbegin(); it != componentOrder_.rend(); ++it) {
			components_[*it]->OnDestroy();
		}
		components_.clear();
		componentOrder_.clear();
	}

	// --- 基本情報 ---------------------------------------------------
	const std::string& GetName() const { return name_; }
	void SetName(std::string name) { name_ = std::move(name); }

	bool IsActive() const { return active_; }
	void SetActive(bool active) { active_ = active; }

	// コンポーネントはここ経由でバスにアクセスする:
	//   GetOwner()->GetEventBus()->Publish(...)
	// バスを持たないGameObject(テスト用など)ではnullptrを返す。
	EventBus* GetEventBus() const { return eventBus_; }

private:
	std::string name_;
	bool active_ = true;
	EventBus* eventBus_ = nullptr;

	std::unordered_map<ComponentTypeId, std::unique_ptr<ComponentBase>> components_;
	std::vector<ComponentTypeId> componentOrder_;  // Update順を安定させるため
};