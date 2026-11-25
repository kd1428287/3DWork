#pragma once

class EnemySpawner;
class GameObject;
class PlayerStatusController;

// 各Stateが参照する依存をまとめたもの(所有はしない)
struct TutorialContext
{
	GameObject* player = nullptr;
	PlayerStatusController* playerStatus = nullptr;
	EnemySpawner* enemySpawner = nullptr;
	// TODO: UI表示、時間速度の制御の窓口を追加する
};
