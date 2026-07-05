#pragma once
// ============================================================
// カードが現在「選択中(アクティブ)」かどうかだけを保持するコンポーネント。
// 判定ロジックは一切持たない(=CardSelectionSystemが外から書き込むだけの
// データ置き場)。これはPlayerInputComponentと同じ立ち位置で、
// 「入力/選択の結果を受け取って保持するだけ」の役割に徹する。
// ============================================================
class CardSelectionComponent : public ComponentBase {
public:
	explicit CardSelectionComponent(GameObject* owner) : ComponentBase(owner) {}

	bool IsActive() const { return active_; }
	void SetActive(bool active) { active_ = active; }

private:
	bool active_ = false;
};