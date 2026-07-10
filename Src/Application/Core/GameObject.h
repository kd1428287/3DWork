#pragma once

#include "SceneContext.h"
#include "../Components/ComponentTypeID.h"
#include "../Components/Tags/TagInterfaces.h"

// ============================================================
// GameObject: コンポーネントの入れ物。
// 型ごとに1インスタンスまで(Unityの基本挙動と同様)を前提とする。
//
// 「特定のタグインターフェース(IRenderable等)を実装している
// コンポーネントだけを効率よく走査する」仕組みは汎用化されており、
// TagInterfaces.h に列挙した型であれば、GameObject自体を変更せずに
// 自動で登録・除去・走査の対象になる。
// ============================================================
class GameObject {
public:
	// contextはSceneが所有する非所有参照の束。
	// eventBus/activeCameraのように、Sceneに1つだけの既知の対象が
	// 増えるたびにGameObject自体を変更しなくて済むよう、
	// 1つのポインタにまとめて持つ。
	explicit GameObject(std::string name = "GameObject", SceneContext* context = nullptr)
		: name_(std::move(name)), context_(context) {}

	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;

	~GameObject() { DestroyAllComponents(); }

	// --- コンポーネント操作 ----------------------------------------

	template <typename T, typename... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of_v<ComponentBase, T>,
			"T must derive from ComponentBase");

		const ComponentTypeId id = GetComponentTypeId<T>();
		auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* rawPtr = component.get();
		ComponentBase* base = rawPtr;

		components_[id] = std::move(component);
		componentOrder_.push_back(id);

		// TがKD_TAG_INTERFACESに列挙されたどのタグを実装しているかを
		// コンパイル時に判定し、該当するものだけタグレジストリに登録する。
		RegisterTags<T, TAG_INTERFACES>(rawPtr, base);

		rawPtr->Awake();
		return rawPtr;
	}

	template <typename T>
	T* GetComponent() const {
		auto it = components_.find(GetComponentTypeId<T>());
		if (it == components_.end()) return nullptr;
		return static_cast<T*>(it->second.get());
	}

	template <typename T>
	bool HasComponent() const {
		return components_.find(GetComponentTypeId<T>()) != components_.end();
	}

	template <typename T>
	void RemoveComponent() {
		const ComponentTypeId id = GetComponentTypeId<T>();
		auto it = components_.find(id);
		if (it == components_.end()) return;

		ComponentBase* comp = it->second.get();
		it->second->OnDestroy();

		UnregisterTags<T, TAG_INTERFACES>(comp);

		components_.erase(it);
		componentOrder_.erase(
			std::remove(componentOrder_.begin(), componentOrder_.end(), id),
			componentOrder_.end());
	}

	// --- 動的な付け外し(予約方式・安全) --------------------------------
	// Update()/LateUpdate()はcomponentOrder_をfor文で回しているため、
	// その最中にAddComponent<T>()(push_backによる再確保の可能性)や
	// RemoveComponent<T>()(eraseによるずれ)を直接呼ぶと、外側の
	// イテレータが無効化されクラッシュ/未定義動作の原因になる。
	//
	// 「自分自身や兄弟コンポーネントを、今まさに実行中のUpdateの中で
	// 付け外ししたい」場合は、必ずこちらの予約APIを使うこと。
	// 実際の追加/削除はFlushComponentChanges()が呼ばれるまで遅延される
	// (ObjectManager::Destroy()+Flush()と全く同じ考え方)。
	template <typename T, typename... Args>
	void RequestAddComponent(Args... args) {
		pendingActions_.push_back(
			[this, args...]() { AddComponent<T>(args...); });
	}

	template <typename T>
	void RequestRemoveComponent() {
		pendingActions_.push_back([this]() { RemoveComponent<T>(); });
	}

	// 予約されていた付け外しをまとめて反映する。
	// ObjectManagerがフレームの安全なタイミング(Update/LateUpdateが全て終わった後)
	// で1回だけ呼ぶ想定。
	void FlushComponentChanges() {
		if (pendingActions_.empty()) return;
		// 実行中にさらに新しい予約が積まれる可能性があるので、
		// 一旦取り出してから実行する(実行中のリストへの追記を避ける)。
		auto actions = std::move(pendingActions_);
		pendingActions_.clear();
		for (auto& action : actions) {
			action();
		}
	}

	// --- ライフサイクル駆動 ----------------------------------------

	void PreUpdate(float deltaTime) {
		if (!active_) return;
		for (const auto id : componentOrder_) {
			ComponentBase* comp = components_[id].get();
			if (!comp->IsEnabled()) continue;
			if (!comp->started_) {
				comp->Start();
				comp->started_ = true;
			}
			comp->PreUpdate(deltaTime);
		}
	}

	void Update(float deltaTime) {
		if (!active_) return;
		for (const auto id : componentOrder_) {
			ComponentBase* comp = components_[id].get();
			if (!comp->IsEnabled()) continue;
			comp->Update(deltaTime);
		}
	}

	void PostUpdate(float deltaTime) {
		if (!active_) return;
		for (const auto id : componentOrder_) {
			ComponentBase* comp = components_[id].get();
			if (comp->IsEnabled()) {
				comp->PostUpdate(deltaTime);
			}
		}
	}

	// --- 描画パス駆動 ------------------------------------------------
	// IRenderableというタグの、それぞれのメソッドをForEachTaggedで回すだけ。
	// 新しいパスを増やしても、ForEachTaggedの呼び出しを1行足すだけでよい。

	void PreDraw() { ForEachTagged<IRenderable>(&IRenderable::PreDraw); }
	void GenerateDepthMapFromLight() { ForEachTagged<IRenderable>(&IRenderable::GenerateDepthMapFromLight); }
	void DrawUnLit() { ForEachTagged<IRenderable>(&IRenderable::DrawUnLit); }
	void DrawLit() { ForEachTagged<IRenderable>(&IRenderable::DrawLit); }
	void DrawEffect() { ForEachTagged<IRenderable>(&IRenderable::DrawEffect); }
	void DrawBright() { ForEachTagged<IRenderable>(&IRenderable::DrawBright); }
	void DrawSprite() { ForEachTagged<IRenderable>(&IRenderable::DrawSprite); }
	void DrawDebug() { ForEachTagged<IRenderable>(&IRenderable::DrawDebug); }

	// お試しの2つ目のタグ。GameObject内部のロジックは1行足すだけで済む。
	//void PrintAllDebugInfo() { ForEachTagged<IDebugPrintable>(&IDebugPrintable::PrintDebugInfo); }

	void DestroyAllComponents() {
		for (auto it = componentOrder_.rbegin(); it != componentOrder_.rend(); ++it) {
			components_[*it]->OnDestroy();
		}
		components_.clear();
		componentOrder_.clear();
		tagRegistry_.clear();
	}

	// --- 基本情報 ---------------------------------------------------
	const std::string& GetName() const { return name_; }
	void SetName(std::string name) { name_ = std::move(name); }

	bool IsActive() const { return active_; }
	void SetActive(bool active) { active_ = active; }

	// Sceneが持つ非所有参照の束そのものにアクセスしたい場合はこちら。
	// 例: GetOwner()->GetContext()->activeCamera
	SceneContext* GetContext() const { return context_; }

	// --- 汎用タグ走査 --------------------------------------------------
	// TagInterfaces.hに列挙した任意のタグについて、実装コンポーネントの
	// 一覧が欲しい場合はこちらを直接使うこともできる。
	template <typename Tag>
	std::vector<Tag*> GetTagged() const {
		std::vector<Tag*> result;
		auto it = tagRegistry_.find(std::type_index(typeid(Tag)));
		if (it == tagRegistry_.end()) return result;
		for (auto& entry : it->second) {
			if (entry.component->IsEnabled()) {
				result.push_back(static_cast<Tag*>(entry.taggedPtr));
			}
		}
		return result;
	}

private:
	// タグレジストリの1エントリ。
	// 「有効かどうかの判定(ComponentBase側)」と「実際に呼ぶメソッド
	// (タグ側)」の両方のポインタを、登録時に一度だけ取得しておく。
	struct TaggedEntry {
		ComponentBase* component;
		void* taggedPtr;  // 実際の型はTag*。呼び出し側でstatic_castして使う。
	};

	// --- タグ登録/解除の汎用実装(可変長テンプレート + fold式) -----------
	// KD_TAG_INTERFACESに列挙された型それぞれについて、
	// Tがそれを実装しているかをコンパイル時に判定して登録する。
	// 新しいタグを増やしてもここは一切変更不要。

	template <typename T, typename... Tags>
	void RegisterTags(T* rawPtr, ComponentBase* base) {
		(RegisterOneTag<Tags>(rawPtr, base), ...);
	}

	template <typename Tag, typename T>
	void RegisterOneTag(T* rawPtr, ComponentBase* base) {
		if constexpr (std::is_base_of_v<Tag, T>) {
			tagRegistry_[std::type_index(typeid(Tag))].push_back(
				{ base, static_cast<void*>(static_cast<Tag*>(rawPtr)) });
		}
	}

	template <typename T, typename... Tags>
	void UnregisterTags(ComponentBase* comp) {
		(UnregisterOneTag<Tags, T>(comp), ...);
	}

	template <typename Tag, typename T>
	void UnregisterOneTag(ComponentBase* comp) {
		if constexpr (std::is_base_of_v<Tag, T>) {
			auto it = tagRegistry_.find(std::type_index(typeid(Tag)));
			if (it == tagRegistry_.end()) return;
			auto& list = it->second;
			list.erase(std::remove_if(list.begin(), list.end(),
				[comp](const TaggedEntry& e) { return e.component == comp; }),
				list.end());
		}
	}

	// 指定タグのメソッドを、有効なコンポーネントに対してのみ呼び出す。
	template <typename Tag>
	void ForEachTagged(void (Tag::* method)()) {
		if (!active_) return;
		auto it = tagRegistry_.find(std::type_index(typeid(Tag)));
		if (it == tagRegistry_.end()) return;
		for (auto& entry : it->second) {
			if (entry.component->IsEnabled()) {
				(static_cast<Tag*>(entry.taggedPtr)->*method)();
			}
		}
	}

	std::string name_;
	bool active_ = true;
	SceneContext* context_ = nullptr;

	std::unordered_map<ComponentTypeId, std::unique_ptr<ComponentBase>> components_;
	std::vector<ComponentTypeId> componentOrder_;

	// タグ型ごとの登録リスト。キーはstd::type_index(typeid(Tag))。
	std::unordered_map<std::type_index, std::vector<TaggedEntry>> tagRegistry_;

	// RequestAddComponent/RequestRemoveComponentで積まれた、
	// まだ反映されていない付け外し予約。
	std::vector<std::function<void()>> pendingActions_;
};