#pragma once
#include <string>
#include "CharacterDefinitionCommon.h"

// 被弾側も得るデータ構造体
struct AttackDamageData
{
	float damage = 10.0f;				  // 基本ダメージ量
	float postureDamage = 20.0f;		  // 基本体幹ダメージ量
	float guardDamageRatio = 0.1f;		  // ガード時ダメージレシオ
	float guardPostureDamageRatio = 0.1f; // ガード時体幹ダメージレシオ
	float parryPostureDamage = 40.0f;	  // パリィされた場合の自身への体幹ダメージ
	bool  isParryable = true;			  // パリィ可能かどうか
	bool  isReboundable = true;			  // パリィ時怯むかどうか
};

// 状態(ヒット/ブロック/パリィ)ごとの演出設定。攻撃側が持つ。
struct ReactionEventConfig
{
	bool enableCameraShake = false;
	bool enableHitStop = false;
	bool enableEffect = true;
	bool enableDistortion = false;

	std::string effectName = "";
	std::string SEName = "";

	// SEのピッチ揺らぎの幅(±)。再生ごとに-幅〜+幅の範囲でランダムにずらす。
	// ピッチは-1.0〜1.0(±1.0がちょうど1オクターブ)で、0なら揺らさない。
	float sePitchRandom = 0.0f;

	// true: 衝突法線と反射ベクトルを合成した方向で出す。
	// false: 衝突法線方向で出す。
	bool useReflectDirection = false;

	// 反射エフェクトの合成比率。1.0で法線のみ、0.0で反射のみ。
	float normalBlendRatio = 0.5f;

	float cameraShakeIntensity = 0.0f;
	float hitStopDurationSeconds = 0.0f;
};

// 攻撃側が決める演出データ(SE/エフェクト/ヒットストップ等)。
struct AttackVisualData
{
	// 標準の斬撃に近い既定値。違う攻撃だけ上書きする。
	ReactionEventConfig hit = { .enableHitStop = true, .hitStopDurationSeconds = 0.05f };
	ReactionEventConfig block;
	ReactionEventConfig parry;

	float hitStopDelaySeconds = 0.0f;
};

// 攻撃フェーズ管理データ
struct AttackPhaseData
{
	MotionClipData windup;   // 予備動作の区間と目標秒数
	MotionClipData active;   // 判定持続の区間と目標秒数
	MotionClipData recovery; // 硬直の区間と目標秒数
};

// モーション制御用データ
struct AttackMoveData
{
	float stepDistance = 0.85f;
	float stepDuration = 0.1f;
	float engageDistance = 1.2f;

	Math::Vector3 stepDirection = Math::Vector3::Zero;
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
	AttackVisualData visualData;
	AttackMoveData moveData;
	AttackPhaseData phaseData;
	AttackCancelData cancelData;

	// このAttackのActiveフェーズで有効化するWeaponSetComponent上のスロット名
	std::vector<std::string> weaponSlots = { "Main" };
};
