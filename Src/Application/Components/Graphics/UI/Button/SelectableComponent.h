#pragma once

#include "Application/Systems/SelectableManager.h"

class UITransformComponent;

// UIの選択可能コンポーネント。当たり判定は同じオブジェクトのUITransformの矩形で行い、入力は外部から渡す
class SelectableComponent : public ComponentBase
{
public:
	enum class State { Normal, Hovered, Selected, Pressed, Disabled };
	enum class Direction { Up, Down, Left, Right };

	explicit SelectableComponent(GameObject* owner) :ComponentBase(owner) {}

	// Awake/デストラクタでSelectableManagerへの登録・解除を行う
	~SelectableComponent();
	void Awake() override;

	//===========================================
	// 判定
	//===========================================

	// 2D描画座標の点が矩形内か(回転は考慮しない)
	bool HitTest(const Math::Vector2& point) const;

	//===========================================
	// 入力の受け取り(マウス・キー・パッド共通)
	//===========================================

	// マウスが乗った/外れた
	void SetHovered(bool hovered)
	{
		if (!interactable_ || hovered_ == hovered) return;
		hovered_ = hovered;
		Notify(hovered_ ? UITrigger::HoverEnter : UITrigger::HoverExit);
	}

	// 選択状態にする。他の選択中UIは自動で解除される
	void Select()
	{
		if (!interactable_ || selected_) return;
		if (s_current) s_current->Deselect();
		selected_ = true;
		s_current = this;
		Notify(UITrigger::Select);
	}

	void Deselect()
	{
		if (!selected_) return;
		selected_ = false;
		pressed_ = false;
		if (s_current == this) s_current = nullptr;
		Notify(UITrigger::Deselect);
	}

	// 押下開始。離した時に範囲内なら決定される
	void Press()
	{
		if (interactable_) pressed_ = true;
	}

	void Release(bool inside)
	{
		const bool fire = pressed_ && inside;
		pressed_ = false;
		if (fire) Submit();
	}

	// キー・パッドの決定ボタン用。即座にonClickを呼ぶ
	void Submit()
	{
		if (interactable_) Notify(UITrigger::Click);
	}

	//===========================================
	// ナビゲーション(キー・パッドの上下左右移動)
	//===========================================

	void SetNavigation(Direction dir, SelectableComponent* target) { nav_[(int)dir] = target; }

	// 縦並びメニュー用。上下を相互にリンクする
	static void LinkVertical(SelectableComponent* upper, SelectableComponent* lower)
	{
		if (upper) upper->SetNavigation(Direction::Down, lower);
		if (lower) lower->SetNavigation(Direction::Up, upper);
	}

	// 指定方向の移動先を選択する。操作不能な相手は飛ばす
	bool Navigate(Direction dir)
	{
		SelectableComponent* next = nav_[(int)dir];
		// 循環参照で無限ループしないよう段数を制限する
		for (int i = 0; next && !next->interactable_ && i < 16; ++i) next = next->nav_[(int)dir];
		if (!next || !next->interactable_) return false;

		next->Select();
		return true;
	}

	//===========================================
	// 設定・取得
	//===========================================

	void SetInteractable(bool value)
	{
		interactable_ = value;
		if (!value)
		{
			SetHovered(false);
			Deselect();
			pressed_ = false;
		}
	}

	// 重なったときの優先度。大きいほど手前で、マウスの判定が先に取られる
	void SetLayer(int layer) { layer_ = layer; }
	// 所属グループ。Managerのアクティブグループと一致するものだけが反応する
	void SetGroup(int group) { group_ = group; }
	int GetLayer() const { return layer_; }
	int GetGroup() const { return group_; }

	bool IsInteractable() const { return interactable_; }
	bool IsHovered() const { return hovered_; }
	bool IsSelected() const { return selected_; }
	bool IsPressed() const { return pressed_; }

	// 見た目の切り替え用。優先度は Disabled > Pressed > Selected > Hovered
	State GetState() const
	{
		if (!interactable_) return State::Disabled;
		if (pressed_) return State::Pressed;
		if (selected_) return State::Selected;
		if (hovered_) return State::Hovered;
		return State::Normal;
	}

	// 現在選択中のUI。無ければnullptr
	static SelectableComponent* GetCurrent() { return s_current; }

	// 発火タイミングごとのアクション(データから設定する)。同じタイミングに複数追加でき、追加順に配信される
	void AddAction(UITrigger trigger, const std::string& id, const std::string& param = "")
	{
		actions_[(int)trigger].push_back({ id, param });
	}
	void ClearActions(UITrigger trigger) { actions_[(int)trigger].clear(); }
	const std::vector<UIAction>& GetActions(UITrigger trigger) const { return actions_[(int)trigger]; }

private:
	// SelectableManagerのキューへ積む。実際の発行はUpdate末尾
	void Notify(UITrigger trigger) const
	{
		for (const auto& action : actions_[(int)trigger]) SelectableManager::Instance().Post(trigger, action);
	}

	UITransformComponent* ui_ = nullptr;
	std::vector<UIAction> actions_[(int)UITrigger::Count];

	bool interactable_ = true;
	bool hovered_ = false;
	bool selected_ = false;
	bool pressed_ = false;

	int layer_ = 0;
	int group_ = 0;

	SelectableComponent* nav_[4] = {};

	inline static SelectableComponent* s_current = nullptr;
};