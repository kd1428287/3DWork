#pragma once

// ============================================================
// ObjectManager: GameObjectの集合を管理する。
//
// BaseScene/SceneManagerと同様、各フェーズ(Update/LateUpdate/Flush)は
// 独立したメソッドとして公開し、「いつ・どの順で呼ぶか」の決定権は
// 持たない。呼び出し側(Scene相当)が他のSystemと合わせて順序を
// 組み立てる。
// ============================================================
class ObjectManager {
public:
	// eventBusはSceneが所有するものへの非所有参照。
	// nullptrのままでも動作する(イベントバスを使わない場合)。
	explicit ObjectManager(EventBus* eventBus = nullptr) { context_.eventBus = eventBus; }

	// 新しいGameObjectを生成してObjectManagerに登録する。
	// 生成と同時にSceneContext(イベントバス・アクティブカメラ等)を紐付ける。
	GameObject* Instantiate(const std::string& name = "GameObject") {
		auto obj = std::make_unique<GameObject>(name, &context_);
		GameObject* rawPtr = obj.get();
		objects_.push_back(std::move(obj));
		return rawPtr;
	}

	// 破棄予約。実際の削除は明示的にFlush()を呼んだタイミングで行う。
	void Destroy(GameObject* target) {
		pendingDestroy_.push_back(target);
	}

	// --- 各フェーズ(それぞれ独立して呼び出し可能) --------------------

	void PreUpdate(float deltaTime) {
		// フレーム先頭で生の(スケールされていない)経過時間を記録しておく。
		// 各GameObjectのtimeScale_に関わらず、効果の持続時間カウントなどは
		// 常にこの値を参照する。
		context_.unscaledDeltaTime = deltaTime;

		for (auto& obj : objects_) {
			obj->PreUpdate(deltaTime);
		}
	}

	void Update(float deltaTime) {
		for (auto& obj : objects_) {
			obj->Update(deltaTime);
		}
	}

	void PostUpdate(float deltaTime) {
		for (auto& obj : objects_) {
			obj->PostUpdate(deltaTime);
		}
	}

	// --- 描画パス(それぞれ独立して呼び出し可能) ------------------------
	// 呼ぶ順序はBaseScene::Draw()側が決める。ObjectManagerはただ転送するだけ。

	void PreDraw() { for (auto& obj : objects_) obj->PreDraw(); }
	void GenerateDepthMapFromLight() { for (auto& obj : objects_) obj->GenerateDepthMapFromLight(); }
	void DrawUnLit() { for (auto& obj : objects_) obj->DrawUnLit(); }
	void DrawLit() { for (auto& obj : objects_) obj->DrawLit(); }
	void DrawEffect() { for (auto& obj : objects_) obj->DrawEffect(); }
	void DrawBright() { for (auto& obj : objects_) obj->DrawBright(); }
	void DrawSprite() { for (auto& obj : objects_) obj->DrawSprite(); }
	void DrawDebug() { for (auto& obj : objects_) obj->DrawDebug(); }

	// 破棄予約されたGameObjectをまとめて削除し、続けて生き残った
	// 各GameObjectのコンポーネント付け外し予約も反映する。
	// Scene側が「このフレームの処理は全部終わった」と判断した
	// タイミング(例: PostUpdateの最後)で明示的に呼ぶ。
	void Flush() {
		if (!pendingDestroy_.empty()) {
			objects_.erase(
				std::remove_if(objects_.begin(), objects_.end(),
					[this](const std::unique_ptr<GameObject>& obj) {
						return std::find(pendingDestroy_.begin(),
							pendingDestroy_.end(),
							obj.get()) != pendingDestroy_.end();
					}),
				objects_.end());

			pendingDestroy_.clear();
		}

		// GameObject単位の破棄とは独立して、各GameObject内で予約された
		// コンポーネントの付け外し(RequestAddComponent/RequestRemoveComponent)
		// もここでまとめて反映する。
		for (auto& obj : objects_) {
			obj->FlushComponentChanges();
		}
	}

	std::size_t GetObjectCount() const { return objects_.size(); }

	// Systemがオブジェクト全体を走査したい場合に使う
	// (例: ターン開始時に全カードをアンタップする、など)
	template <typename Func>
	void ForEachObject(Func&& func) {
		for (auto& obj : objects_) {
			func(obj.get());
		}
	}

	// 指定した型のコンポーネントを持つオブジェクトを一括取得する。
	template <typename T>
	std::vector<T*> FindComponents() {
		std::vector<T*> result;
		for (auto& obj : objects_) {
			if (T* comp = obj->GetComponent<T>()) {
				result.push_back(comp);
			}
		}
		return result;
	}

	// 指定した名前を持つオブジェクトを一括取得する。
	std::vector<GameObject*> FindName(const std::string& name) {
		std::vector<GameObject*> result;
		for (auto& obj : objects_) {
			if (obj->GetName() == name) {
				result.push_back(obj.get());
			}
		}
		return result;
	}

private:
	std::vector<std::unique_ptr<GameObject>> objects_;
	std::vector<GameObject*> pendingDestroy_;
	SceneContext context_;  // ObjectManager(Sceneの相当物)がこの実体を所有する
};