#include "TutorialDataTable.h"

namespace TutorialDataTable
{
	TutorialConfig Create()
	{
		TutorialConfig config;
		// TODO: Prefab名、座標、範囲、回数は仮の値なので調整する
		const std::string passive = "TutorialEnemyPassive"; // 攻撃データなし
		const std::string attacker = "TutorialEnemy";

		config.move.messageId = "TutorialMove";
		config.move.targetPosition = Math::Vector3(0.0f, 0.0f, 10.0f);
		config.move.radius = 2.0f;

		config.attack.messageId = "TutorialAttack";
		config.attack.enemies = { { passive, Math::Vector3(0.0f, 0.0f, 15.0f) } };

		config.parry.messageId = "TutorialParry";
		config.parry.enemy = { attacker, Math::Vector3(0.0f, 0.0f, 15.0f) };
		config.parry.requiredCount = 3;
		config.parry.pauseOnPrompt = true;

		config.finalBattle.messageId = "TutorialFinal";
		config.finalBattle.enemies = {
			{ attacker, Math::Vector3(-4.0f, 0.0f, 15.0f) },
			{ attacker, Math::Vector3(0.0f, 0.0f, 18.0f) },
			{ attacker, Math::Vector3(4.0f, 0.0f, 15.0f) },
		};

		return config;
	}
}
