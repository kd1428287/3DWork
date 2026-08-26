#pragma once
#include "../EnemyStatusController.h"

// ============================================================
// BossStatusController
// ボス(名称未定)用の派生クラス。ボス固有の攻撃パターンがまだ
// 決まっていないため、現時点ではEnemyStatusControllerの仮攻撃タイマー
// (一定間隔でHitBoxを有効化するだけ)をそのまま使う状態にしている。
//
// 攻撃パターンが決まったら、下のUpdateAttackTimer()をoverrideして
// 複数パターンの切り替え等をここに実装すること。基底クラスの
// data_(EnemyStatusData)・attackIntervalTimer_/hitBoxActiveTimer_・
// SetWeaponHitBoxEnabled()/CanAttack()はprotectedなので、そのまま
// 流用してもよいし、フェーズ管理用の新しいメンバをここに追加してもよい。
// ============================================================
class BossStatusController : public EnemyStatusController {
public:
	using EnemyStatusController::EnemyStatusController;

protected:
	// TODO: ボスの攻撃パターンが決まったらoverrideする。
	// void UpdateAttackTimer(float deltaTime) override;
};
