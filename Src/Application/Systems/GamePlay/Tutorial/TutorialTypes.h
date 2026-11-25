#pragma once
#include <string>
#include <vector>

// チュートリアル用の設定データ。値はTutorialDataTableで生成する。

struct EnemySpawnData
{
	std::string prefabName;
	Math::Vector3 position;
};

struct ReachAreaData
{
	std::string messageId;
	Math::Vector3 targetPosition;
	float radius = 0.0f;
};

struct DefeatEnemiesData
{
	std::string messageId;
	std::vector<EnemySpawnData> enemies;
};

struct ParryData
{
	std::string messageId;
	EnemySpawnData enemy;
	int requiredCount = 0;
	bool pauseOnPrompt = true; // 説明表示中に時間を止めるか
};

struct TutorialConfig
{
	ReachAreaData move;
	DefeatEnemiesData attack;
	ParryData parry;
	DefeatEnemiesData finalBattle;
};
