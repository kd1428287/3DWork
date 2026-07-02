#pragma once

#include "GameObject.h"

// ============================================================
// ObjectManager: GameObjectの集合を管理し、毎フレームのUpdate/LateUpdateを
// 一括で駆動する。破棄予約されたGameObjectはフレーム末にまとめて削除する。
// ============================================================
class ObjectManager {
public:
	// 新しいGameObjectを生成してObjectManagerに登録する。
	GameObject* Instantiate(const std::string& name = "GameObject") {
		auto obj = std::make_unique<GameObject>(name);
		GameObject* rawPtr = obj.get();
		objects_.push_back(std::move(obj));
		return rawPtr;
	}

	// 破棄予約。実際の削除はフレーム末(Flush)でまとめて行う。
	void Destroy(GameObject* target) {
		pendingDestroy_.push_back(target);
	}

	void Update(float deltaTime) {
		for (auto& obj : objects_) {
			obj->Update(deltaTime);
		}
		for (auto& obj : objects_) {
			obj->LateUpdate(deltaTime);
		}
		Flush();
	}

	std::size_t GetObjectCount() const { return objects_.size(); }

private:
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

	std::vector<std::unique_ptr<GameObject>> objects_;
	std::vector<GameObject*> pendingDestroy_;
};