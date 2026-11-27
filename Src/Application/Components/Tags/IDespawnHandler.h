#pragma once

// 破棄要求を受けて後始末まで行うコンポーネントのタグ。持たないオブジェクトは即Destroyされる。
class IDespawnHandler
{
public:
	virtual ~IDespawnHandler() = default;

	// 道連れ・演出の後始末を済ませ、自分のオブジェクトの破棄を予約する。
	virtual void RequestDespawn() = 0;
};
