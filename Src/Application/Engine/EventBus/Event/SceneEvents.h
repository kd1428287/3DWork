#pragma once
#include "Event.h"

// ※ SceneType.hへの実際のパスはプロジェクト構成に合わせて調整してください
//    (例: Scenes/フォルダが Event.h からどのくらい上にあるか)
#include "../../../Scene/SceneType.h"

namespace Events
{
	namespace Scene
	{
		// 「指定のシーンへ遷移してほしい」という、発行者を問わない汎用イベント。
		//
		// 「特定の敵を倒したら」「タイマーが切れたら」等、"何が遷移条件か"という
		// ゲームルール固有の判断は、このイベントをPublishする側
		// (敵のコンポーネント/システムなど)だけが持つ。
		// 受け手(SceneManager)はSceneTypeを見てSetNextSceneするだけでよく、
		// 遷移理由が増えてもSceneManager側は無改造で済む。
		//
		// 明示的にコンストラクタを持たせているのは、Eventという基底クラスが
		// あるため { SceneType::Game } のような集成体初期化がC++17未満の
		// 設定だと使えず、コンパイルエラーになるため
		// (function-style castとして解釈され、対応するコンストラクタが
		// ないと失敗する)。
		struct SceneChangeRequestEvent : public Event
		{
			SceneType nextScene;

			explicit SceneChangeRequestEvent(SceneType scene)
				: nextScene(scene)
			{}
		};
	}
}