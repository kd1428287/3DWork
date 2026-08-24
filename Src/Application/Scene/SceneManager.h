#pragma once

#include "SceneType.h"

class BaseScene;
class ScopedSubscriber; // EventBus.h (前方宣言のみ。実体はEventBus.h参照)

class SceneManager
{
public:

	void Update();

	void PreDraw();
	void Draw();
	void DrawSprite();
	void DrawDebug();

	// 次のシーンをセット (次のフレームから切り替わる)
	void SetNextScene(SceneType _nextScene)
	{
		m_nextSceneType = _nextScene;
	}
private:

	// マネージャーの初期化
	// インスタンス生成(アプリ起動)時にコンストラクタで自動実行
	void Init();

	// シーン切り替え関数
	void ChangeScene(SceneType _sceneType);

	// 現在のシーンのインスタンスを保持しているポインタ
	std::shared_ptr<BaseScene> m_currentScene = nullptr;

	// 現在のシーンの種類を保持している変数
	SceneType m_currentSceneType = SceneType::Game;

	// 次のシーンの種類を保持している変数
	SceneType m_nextSceneType = m_currentSceneType;

	// Events::Scene::SceneChangeRequestEvent の購読。
	// 「どの敵を倒したら」等の遷移理由はここでは一切判断せず、
	// 受け取ったSceneTypeへSetNextSceneするだけの薄い受け口にする。
	// unique_ptrにしているのはSceneManager.hにEventBus.hをincludeさせないため
	// (前方宣言のみで済ませる)。
	std::unique_ptr<ScopedSubscriber> m_sceneChangeSub;

private:

	SceneManager() { Init(); }
	~SceneManager();

public:

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
