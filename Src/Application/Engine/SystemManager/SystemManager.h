#pragma once

#include <vector>
#include <functional>

// ループ中の実行順を管理する
class SystemManager {
public:
	// 登録された順番で、各関数を呼び出す
	// deltaTimeは登録時ではなくここで渡すので、毎フレーム変わる値をそのまま反映できる
	void Update(float deltaTime)
	{
		for (auto& func : executionOrder_)
		{
			func(deltaTime);
		}
	}

	// 渡した「呼び出し可能なもの（関数、ラムダなど）」を、渡した順番に実行するよう登録する
	// 各callableは float(deltaTime) を1つ引数に取る形で書く
	// 例: systemManager_->SetExecutionOrder(
	//         [this](float dt) { inputSystem_->Update(dt); },
	//         [this](float dt) { colliderRegistry_->Refresh(*objManager_); },
	//         [this](float dt) { collisionSystem_->Update(*colliderRegistry_); }
	//     );
	template <typename... Funcs>
	void SetExecutionOrder(Funcs&&... funcs)
	{
		executionOrder_.clear();
		executionOrder_.reserve(sizeof...(Funcs));

		// フォールド式でパラメータパックを展開し、各callableを順番にvectorへ積んでいく
		(executionOrder_.emplace_back(std::forward<Funcs>(funcs)), ...);
	}

private:
	std::vector<std::function<void(float)>> executionOrder_;
};