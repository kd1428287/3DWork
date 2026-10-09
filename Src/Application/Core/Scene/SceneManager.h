#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include "SceneType.h"
#include "Application/Core/EventBus/EventBus.h" // ScopedSubscriberを値メンバで持つため

class BaseScene;

class SceneManager
{
public:

	void Update();

	void PreDraw();
	void Draw();
	void DrawSprite();
	void DrawDebug();

	// 次のシーンをセット (次のフレーム先頭で切り替わる)
	// 現在と同じシーンかつ予約なしの場合は何もしない
	void SetNextScene(SceneType nextScene)
	{
		if (nextScene == currentSceneType_ && !pending_) return;
		pending_ = nextScene;
	}

	// 現在のシーンを作り直す (次のフレーム先頭で実行。遷移予約があればそちらを優先)
	void ReloadScene()
	{
		if (!pending_) pending_ = currentSceneType_;
	}

	// 明示的な終了処理。エンジン系シングルトンの破棄より前に呼ぶ(複数回呼んでも安全)
	void Shutdown();

private:

	using SceneFactory = std::function<std::unique_ptr<BaseScene>()>;

	// ファクトリ登録とイベント購読。シーン生成は最初のUpdate()で行う
	void Init();

	// 予約された遷移を実行する。必ずUpdate()の先頭から呼ぶ
	void ApplyPendingTransition();

	// 現在のシーンを Exit() してから破棄する
	void DestroyCurrentScene();

	// 現在のシーンのインスタンス(単一所有)
	std::unique_ptr<BaseScene> currentScene_ = nullptr;

	// シーン種類 → 生成関数
	std::unordered_map<SceneType, SceneFactory> factories_;

	// 現在のシーンの種類を保持している変数
	SceneType currentSceneType_ = SceneType::Game;

	// 遷移の予約(未予約ならnullopt)。開始シーンもこの経路で生成する
	std::optional<SceneType> pending_;

	// イベントの購読
	ScopedSubscriber sceneChangeSub_;
	ScopedSubscriber reloadSceneSub_;

private:

	// 不完全型のunique_ptrを持つため、定義はcpp側に置く
	SceneManager();
	~SceneManager();

public:

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	// シングルトンパターン
	// 常に存在する && 必ず1つしか存在しない(1つしか存在出来ない)
	// どこからでもアクセスが可能で便利だが
	// 何でもかんでもシングルトンという思考はNG
	static SceneManager& Instance()
	{
		static SceneManager instance;
		return instance;
	}
};