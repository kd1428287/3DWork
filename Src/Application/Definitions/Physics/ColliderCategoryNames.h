#pragma once
#include "../Physics/ColliderCategory.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ColliderCategoryの各ビットと、json保存/UI表示用の名前("Ground"等)の対応表。
//	MapEditor.cpp(チェックボックスUI)とComponentRegistrations.cpp(実体化)の
//	両方がここを参照する。カテゴリを増減させる時はここ1箇所を直せばよい
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
inline const std::vector<std::pair<const char*, ColliderCategory>>& GetColliderCategoryFlags()
{
	static const std::vector<std::pair<const char*, ColliderCategory>> flags = {
		{ "Ground",    ColliderCategory::Ground },
		{ "Bump",      ColliderCategory::Bump },
		{ "HitBox",    ColliderCategory::HitBox },
		{ "HitLine",   ColliderCategory::HitLine },
		{ "HurtBox",   ColliderCategory::HurtBox },
		{ "Sight",     ColliderCategory::Sight },
		{ "EventArea", ColliderCategory::EventArea },
	};
	return flags;
}

// ColliderCategoryのビット集合 → 有効なフラグ名の配列(json保存用)
inline std::vector<std::string> ColliderCategoryToNames(ColliderCategory mask)
{
	std::vector<std::string> names;
	for (auto& kv : GetColliderCategoryFlags())
	{
		if (Any(mask & kv.second)) { names.push_back(kv.first); }
	}
	return names;
}

// フラグ名の配列 → ColliderCategoryのビット集合(json読み込み用)。
// 知らない名前は無視する(将来カテゴリの名称が変わっても読み込み自体は落とさない)
inline ColliderCategory ColliderCategoryFromNames(const std::vector<std::string>& names)
{
	ColliderCategory mask = ColliderCategory::None;
	for (auto& n : names)
	{
		for (auto& kv : GetColliderCategoryFlags())
		{
			if (n == kv.first) { mask |= kv.second; break; }
		}
	}
	return mask;
}
