#pragma once
#include "../EnemyStatusController.h"

// ============================================================
// BruteStatusController
// 雑魚敵(Brute)。現状はEnemyStatusControllerの挙動(パトロール+
// 一定間隔でHitBoxを有効化するだけの仮攻撃AI)をそのまま使うだけの
// 薄い派生クラス。EnemyFactory側でBrute用のGameObjectにアタッチする
// クラスをEnemyStatusControllerと型として区別するために用意している
// (将来Brute固有の挙動が増えたら、UpdateAttackTimer()等をここで
// overrideする)。
// ============================================================
class BruteStatusController : public EnemyStatusController {
public:
	using EnemyStatusController::EnemyStatusController;
};
