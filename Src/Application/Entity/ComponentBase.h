#pragma once

class GameObject;

// 全コンポーネントの基底クラス
class ComponentBase {
public:
	explicit ComponentBase(GameObject* owner)
		: owner_(owner), generation_(s_nextGeneration++) {}
	virtual ~ComponentBase() = default;

	// コピー・ムーブは基本的に禁止(GameObjectが所有権を管理するため)
	ComponentBase(const ComponentBase&) = delete;
	ComponentBase& operator=(const ComponentBase&) = delete;

	// --- ライフサイクルフック -----------------------------------
	// 最初のUpdateの前に一度だけ呼ばれる(他コンポーネントの参照解決などに使う)
	virtual void Awake() {}
	// 毎F最初の更新前に呼ばれる
	virtual void Start() {}
	// Update前 入力の取得等
	virtual void PreUpdate(float deltaTime) { (void)deltaTime; }
	// 毎フレーム呼ばれる　メイン処理
	virtual void Update(float deltaTime) { (void)deltaTime; }
	// Update後、描画前などに呼ばれる　トランスフォーム同期等
	virtual void PostUpdate(float deltaTime) { (void)deltaTime; }
	// コンポーネントが破棄される直前に呼ばれる
	virtual void OnDestroy() {}

	// --- 共通アクセサ --------------------------------------------
	GameObject* GetOwner() const { return owner_; }

	bool IsEnabled() const { return enabled_; }
	void SetEnabled(bool enabled) { enabled_ = enabled; }

	// このコンポーネントインスタンス固有の世代番号。生成時に、全コンポーネント
	// (型やGameObjectをまたいだ)通し番号として自動的に発行される。0は
	// 使用しない(未初期化のハンドルと衝突させないための予約)ため1から始まる。
	//
	// フレームをまたいで生ポインタ(T*)を保持し続けるコード
	// (CollisionSystemの重なりペア記録など)は、Handle<T>(Handle.h)経由で
	// これを使い、「破棄後に別のコンポーネントへポインタ値が再利用されて
	// いないか」を検証できる。
	uint64_t GetGeneration() const { return generation_; }

private:
	GameObject* owner_ = nullptr;
	bool enabled_ = true;
	uint64_t generation_ = 0;

	// 全ComponentBase派生インスタンスで共有される、生成順の通し番号カウンタ。
	// このプロジェクトはGameObject/ComponentBaseの生成・破棄を単一の
	// Updateループ内(単一スレッド)でのみ行う前提のため、素のstaticで十分
	// (atomicにすると「マルチスレッドから生成されうる」という誤った
	//  前提を読み手に与えてしまうため、あえて使わない)。
	static inline uint64_t s_nextGeneration = 1;

	friend class GameObject;  // started_ 管理のためGameObjectからのみ触れる
};