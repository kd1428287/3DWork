#pragma once

// シーンの種類を表す列挙型。
// 以前はSceneManagerのネストされた列挙型(SceneManager::SceneType)だったが、
// イベント経由でのシーン遷移要求(Events::Scene::SceneChangeRequestEvent)から
// SceneManager全体をincludeしなくて済むよう、独立したヘッダへ切り出した。
// (Component/Event層がScene管理層全体に依存するのを避けるため)
enum class SceneType
{
	Title,
	Game,
	Result,
};
