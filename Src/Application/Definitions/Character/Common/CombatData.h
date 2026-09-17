#pragma once


// 被弾側も得るデータ構造体
struct AttackDamageData
{
	float damage = 10.0f;
	float knockbackPower = 6.0f;
	float hitStunSeconds = 0.5f;

	// この攻撃をガードされた時に、被弾側の体幹(PostureComponent)へ
	// 与えるダメージ量。
	float postureDamage = 20.0f;

	// ガード時のチップダメージ(HP)の割合。damage * chipDamageRatioが
	// 実際にHealthComponentへ適用される(通常被弾のdamageそのものより
	// 大幅に軽減される想定)。
	float chipDamageRatio = 0.1f;

	// この攻撃がジャスト(パリィ)で受けられた時に、攻撃者側の
	// 体幹(PostureComponent)へ与えるダメージ量。通常のpostureDamageより
	// 大きくするのが基本(弾き返された側が大きく怯む、という設計のため)。
	float parryPostureDamage = 40.0f;
};

struct AnimationSegment
{
	float startFrame = 0.0f;     // 切り出し開始フレーム
	float endFrame = 0.0f;       // 切り出し終了フレーム
	float targetDuration = 0.0f; // この区間を何秒で再生しきりたいか（デザイナーの意図するテンポ）
};

// 攻撃フェーズ管理データ
struct AttackPhaseData
{
	std::string animationName = "APose_Attack02_1"; // 元となるアニメーションクリップ名

	AnimationSegment windup;   // 予備動作の区間と目標秒数
	AnimationSegment active;   // 判定持続の区間と目標秒数
	AnimationSegment recovery; // 硬直の区間と目標秒数
};

// モーション制御用データ
struct AttackMoveData
{
	float stepDistance = 0.85f;
	float stepDuration = 0.1f;
	float engageDistance = 1.2f;

	Math::Vector3 stepDirection = Math::Vector3::Zero;
	float blendDuration = 0.1f;
	bool useRootMotion = false;
};

// キャンセル制御用データ
struct AttackCancelData
{
	// Recoveryフェーズ内で、何秒から受け付け開始するか
	float recoveryMoveCancelStart = 0.15f;
	float recoveryEvadeCancelStart = 0.15f;
	float recoveryAttackCancelStart = 0.2f;
	float comboWindowAfterRecovery = 0.0f;
};

// 攻撃行動全体のデータ構造体
struct AttackData
{
	AttackDamageData damageData;
	AttackMoveData moveData;
	AttackPhaseData phaseData;
	AttackCancelData cancelData;

	// このAttackのActiveフェーズで有効化するWeaponSetComponent上のスロット名
	std::vector<std::string> weaponSlots = { "Main" };
};