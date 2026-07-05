#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

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
	explicit ObjectManager(EventBus* eventBus = nullptr) : eventBus_(eventBus) {}

	// 新しいGameObjectを生成してObjectManagerに登録する。
	// 生成と同時にSceneのイベントバスを紐付ける。
	GameObject* Instantiate(const std::string& name = "GameObject") {
		auto obj = std::make_unique<GameObject>(name, eventBus_);
		GameObject* rawPtr = obj.get();
		objects_.push_back(std::move(obj));
		return rawPtr;
	}

	// 破棄予約。実際の削除は明示的にFlush()を呼んだタイミングで行う。
	void Destroy(GameObject* target) {
		pendingDestroy_.push_back(target);
	}

	// --- 各フェーズ(それぞれ独立して呼び出し可能) --------------------

	void Update(float deltaTime) {
		for (auto& obj : objects_) {
			obj->Update(deltaTime);
		}
	}

	void LateUpdate(float deltaTime) {
		for (auto& obj : objects_) {
			obj->LateUpdate(deltaTime);
		}
	}

	// --- 描画パス(それぞれ独立して呼び出し可能) ------------------------
	// 呼ぶ順序はBaseScene::Draw()側が決める。ObjectManagerはただ転送するだけ。

	void GenerateDepthMapFromLight() { for (auto& obj : objects_) obj->GenerateDepthMapFromLight(); }
	void DrawUnLit() { for (auto& obj : objects_) obj->DrawUnLit(); }
	void DrawLit() { for (auto& obj : objects_) obj->DrawLit(); }
	void DrawEffect() { for (auto& obj : objects_) obj->DrawEffect(); }
	void DrawBright() { for (auto& obj : objects_) obj->DrawBright(); }
	void DrawSprite() { for (auto& obj : objects_) obj->DrawSprite(); }
	void DrawDebug() { for (auto& obj : objects_) obj->DrawDebug(); }

	// 破棄予約されたGameObjectをまとめて削除する。
	// Scene側が「このフレームの処理は全部終わった」と判断した
	// タイミング(例: PostUpdateの最後)で明示的に呼ぶ。
	void Flush() {
		if (pendingDestroy_.empty()) return;

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

private:
	std::vector<std::unique_ptr<GameObject>> objects_;
	std::vector<GameObject*> pendingDestroy_;
	EventBus* eventBus_ = nullptr;
};