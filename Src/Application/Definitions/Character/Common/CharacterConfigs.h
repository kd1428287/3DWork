#pragma once

// HitReactionComponentが発生させる副作用のうち、有効/無効を切り替え
// たいだけのもの(強さ等の数値は持たない)をビットフラグでまとめる。
// ObjectFlagsと同じ流儀(ビット単位のenum class + 演算子オーバーロード)。
enum class HitReactionFlags : uint32_t
{
	None = 0,
	CameraShake = 1u << 0, // 通常被弾時にカメラシェイクを発生させるか
	HitStop = 1u << 1, // 通常被弾時にヒットストップを発生させるか
	WeaponClashFx = 1u << 2, // ガード/パリィ時に鍔迫り合いエフェクトを出すか
};

inline HitReactionFlags operator|(HitReactionFlags a, HitReactionFlags b)
{
	return static_cast<HitReactionFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline HitReactionFlags& operator|=(HitReactionFlags& a, HitReactionFlags b)
{
	a = a | b;
	return a;
}
inline bool HasFlag(HitReactionFlags flags, HitReactionFlags test)
{
	return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
}

struct HitReactionConfig
{
	// デフォルト値は、この構造体を導入する以前のPlayer側の挙動
	// (常に全て有効)をそのまま踏襲している。Enemy等、新たに
	// HitReactionComponentを使う側もこのデフォルトのままで既存同等の
	// 見た目になる。
	HitReactionFlags effectFlags = HitReactionFlags::CameraShake | HitReactionFlags::HitStop | HitReactionFlags::WeaponClashFx;

	// 通常被弾時、SpawnDamageEffect()が発生させるエフェクト名
	std::string damageEffectName = "BloodSplatter";

	float cameraShakeIntensity = 0.75f;
	float hitStopDelaySeconds = 0.0f;
	float hitStopDurationSeconds = 0.1f;

	// ガード時のノックバック強度(旧HitReactionComponent::kGuardKnockbackPower)。
	float guardKnockbackPower = 2.0f;

	// 体幹が壊れた(崩し発生)時の怯み秒数として、EnterStaggerの引数へ渡す値
	float largeStaggerDuration = 0.6f;
};

struct CharacterConfig
{
	HitReactionConfig hitReaction_;
};