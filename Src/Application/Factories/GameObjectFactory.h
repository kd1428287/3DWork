#pragma once
// ============================================================
// 汎用オブジェクト生成レジストリ。
//
// 「キー文字列」と「生成関数」を対応付けて登録しておき、
// あとはキー指定だけでGameObjectを組み立てられるようにする。
// PlayerFactory / EnemyFactory / CardFactory のような個別Factoryを
// 都度ゼロから書かなくても、この上に乗せるだけで済む。
//
// Args... には「生成時に外から渡したい実行時パラメータ」を指定する。
// 例:
//   GameObjectFactory<int>            → 所有者プレイヤーIDだけ渡したい(カード等)
//   GameObjectFactory<>               → 追加パラメータ不要(固定のプレハブ等)
//   GameObjectFactory<Vector3, float> → 出現位置と初期HPを渡したい、等
// ============================================================
template <typename... Args>
class GameObjectFactory {
public:
	using CreatorFunc = std::function<GameObject* (ObjectManager&, Args...)>;

	// 生成ロジックを登録する。同じキーで再登録すると上書きされる。
	void Register(const std::string& key, CreatorFunc creator) {
		creators_[key] = std::move(creator);
	}

	// 登録済みキーからGameObjectを生成する。未登録ならnullptr。
	GameObject* Create(const std::string& key, ObjectManager& objectManager, Args... args) const {
		auto it = creators_.find(key);
		if (it == creators_.end()) return nullptr;
		return it->second(objectManager, args...);
	}

	bool IsRegistered(const std::string& key) const {
		return creators_.find(key) != creators_.end();
	}

private:
	std::unordered_map<std::string, CreatorFunc> creators_;
};