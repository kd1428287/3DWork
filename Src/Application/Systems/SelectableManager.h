#pragma once

#include <string>
#include <vector>

class SelectableComponent;

// 全SelectableComponentのマウス・キー入力をまとめて処理する
// KdInputManager::Update後・描画前に毎フレームUpdate()を呼ぶ
class SelectableManager
{
public:
	static SelectableManager& Instance()
	{
		static SelectableManager instance;
		return instance;
	}

	void Update();

	// SelectableComponentのAwake/デストラクタから呼ばれる
	void Register(SelectableComponent* sel);
	void Unregister(SelectableComponent* sel);

	// 反応させるグループ。-1なら全グループ。切り替え時にホバー・押下は解除される
	void SetActiveGroup(int group);
	int GetActiveGroup() const { return activeGroup_; }

	// KdInputManagerに登録済みのボタン名
	void SetButtonNames(const std::string& confirm, const std::string& up, const std::string& down,
		const std::string& left, const std::string& right);

	// trueならマウスが乗ったUIをそのまま選択状態にする
	void SetHoverSelects(bool enable) { hoverSelects_ = enable; }

	bool IsMouseMode() const { return mouseMode_; }

private:
	SelectableManager() = default;

	bool IsActive(const SelectableComponent* sel) const;
	// カーソル下の最前面のUI。無ければnullptr
	SelectableComponent* FindHit(const Math::Vector2& mouse) const;
	SelectableComponent* FindFirstActive() const;
	void SetHover(SelectableComponent* next);
	void ClearHoverAndPress();

	std::vector<SelectableComponent*> list_;

	SelectableComponent* hovered_ = nullptr;
	SelectableComponent* pressed_ = nullptr;

	int			activeGroup_ = -1;
	bool		hoverSelects_ = true;
	bool		mouseMode_ = true;

	Math::Vector2 prevMouse_ = { 0.0f, 0.0f };
	bool		hasPrevMouse_ = false;

	std::string confirmName_ = "UIConfirm";
	std::string upName_ = "UIUp";
	std::string downName_ = "UIDown";
	std::string leftName_ = "UILeft";
	std::string rightName_ = "UIRight";
};
