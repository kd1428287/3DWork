#pragma once

class ISyncroAnimation {
public:
	virtual ~ISyncroAnimation() = default;

	// 致命(被致命)アニメーションを開始する。CanBeFatalized()がtrueの時だけ有効。
	virtual void NotifyLethalAnimation() = 0;

	// 致命を受け付けられる状態か(崩れ中か)。既定は不可(他の実装クラスを壊さないため非純粋)。
	virtual bool CanBeFatalized() const { return false; }
};