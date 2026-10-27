#pragma once

#include <algorithm>
#include <vector>
#include "Application/Components/Physics/Collision/ColliderComponent.h" 

// ============================================================
// Player/Enemy等、複数キャラクターのFactory/Definitionで共通して
// 使うデータ構造をまとめたヘッダ。
// キャラクター種別に依存しない「コライダー1本」「IKチェーン」「武器」の
// 定義をここに集約し、PlayerDefinition/EnemyDefinition等はこれを
// 部品として組み合わせる。
// ============================================================

// カプセルコライダー1本分の定義（Body / HurtBox 等）
struct CapsuleColliderDefinition
{
	std::string name;
	float radius = 0.0f;
	Math::Vector3 start;
	Math::Vector3 end;
	ColliderCategory category = ColliderCategory::Bump;

	// HurtBoxのように「自分がどのカテゴリと衝突判定を取るか」を絞る場合はここに指定。
	// 指定不要なら ColliderCategory::None のままにしておく。
	ColliderCategory interactsWith = ColliderCategory::None;

	bool isTrigger = false;
};

// TwoBoneIKComponentに渡す4ボーン分の定義
struct IKChainDefinition
{
	std::string rootBone;
	std::string midBone;
	std::string tipParentBone;
	std::string tipBone;
};

struct WeaponColliderDefinition
{
	Math::Vector3 halfExtents;
	Math::Vector3 offset;
};

struct WeaponDefinition
{
	std::string modelPath;

	Math::Vector3 socketLocalPosition;
	// Yaw, Pitch, Roll（度数）。
	// 注意: 既存実装ではYawだけ度数のままCreateFromYawPitchRollに渡され、
	// Pitchのみ XMConvertToRadians を通す非対称な呼び出しになっている
	// （PlayerFactory.cpp参照）。データ化にあたっても見た目を変えないよう、
	// 挙動はそのまま踏襲している。直すなら別タスクとして意図的に。
	Math::Vector3 socketLocalEulerRotationDeg;

	WeaponColliderDefinition hitBox;
};

enum class SegmentEase { Linear, EaseIn, EaseOut, EaseInOut };

// フェーズ内の1区間。from>toで逆再生、from==toで停止(溜め)
struct MotionSegment
{
	float		from = 0.0f;	// 再生位置の開始(0〜1)
	float		to = 1.0f;	// 再生位置の終了(0〜1)
	float		weight = 1.0f;	// duration内での相対的な長さ
	SegmentEase	ease = SegmentEase::Linear;
};

inline float ApplySegmentEase(float u, SegmentEase ease)
{
	switch (ease) {
	case SegmentEase::EaseIn:		return u * u;
	case SegmentEase::EaseOut:		return 1.0f - (1.0f - u) * (1.0f - u);
	case SegmentEase::EaseInOut:	return u * u * (3.0f - 2.0f * u);
	default:						return u;
	}
}

// アニメーション1本分の再生設定
struct MotionClipData
{
	std::string animationName;
	float		duration = -1.0f;
	bool		loop = false;
	bool		useRootMotion = false;
	float		blendDuration = 0.1f;
	int			startFrame = 0;
	int			endFrame = 0;
	bool		isForce = true;

	// 空なら従来の等速再生。あればdurationを各weightの比で分配して再生する
	std::vector<MotionSegment> segments;

	// 経過秒 → 再生位置(0〜1)。durationが無効なら先頭を返す
	float EvaluatePosition(float elapsed) const
	{
		if (duration <= 0.0f) { return 0.0f; }
		if (segments.empty()) { return std::clamp(elapsed / duration, 0.0f, 1.0f); }

		float totalWeight = 0.0f;
		for (const auto& s : segments) { totalWeight += s.weight; }
		if (totalWeight <= 0.0f) { return segments.back().to; }

		float remain = elapsed / duration * totalWeight;
		for (const auto& s : segments) {
			if (remain < s.weight) {
				return s.from + (s.to - s.from) * ApplySegmentEase(remain / s.weight, s.ease);
			}
			remain -= s.weight;
		}
		return segments.back().to;
	}
};