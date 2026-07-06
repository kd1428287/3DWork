#pragma once

class GameObject;

// ============================================================
// 全コンポーネントの基底クラス。
// GameObjectにアタッチされ、ライフサイクルフックを通じて
// 振る舞いを実装する。派生クラスは必要なフックだけ override する。
// ============================================================
class ComponentBase {
public:
	explicit ComponentBase(GameObject* owner) : owner_(owner) {}
	virtual ~ComponentBase() = default;

	// コピー・ムーブは基本的に禁止(GameObjectが所有権を管理するため)
	ComponentBase(const ComponentBase&) = delete;
	ComponentBase& operator=(const ComponentBase&) = delete;

	// --- ライフサイクルフック -----------------------------------
	// GameObjectに追加された直後、一度だけ呼ばれる
	virtual void Awake() {}
	// 最初のUpdateの前に一度だけ呼ばれる(他コンポーネントの参照解決などに使う)
	virtual void Start() {}
	// Update前
	virtual void PreUpdate(float deltaTime) { (void)deltaTime; }
	// 毎フレーム呼ばれる
	virtual void Update(float deltaTime) { (void)deltaTime; }
	// Update後、描画前などに呼ばれる
	virtual void PostUpdate(float deltaTime) { (void)deltaTime; }
	// コンポーネントが破棄される直前に呼ばれる
	virtual void OnDestroy() {}

	// --- 共通アクセサ --------------------------------------------
	GameObject* GetOwner() const { return owner_; }

	bool IsEnabled() const { return enabled_; }
	void SetEnabled(bool enabled) { enabled_ = enabled; }

private:
	GameObject* owner_ = nullptr;
	bool enabled_ = true;
	bool started_ = false;

	friend class GameObject;  // started_ 管理のためGameObjectからのみ触れる
};