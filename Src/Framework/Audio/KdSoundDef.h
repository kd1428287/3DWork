#pragma once

#include "Application/Core/EventBus/Events/AudioEvents.h"

// 論理IDに対応する音の定義
struct SoundDef
{
	std::vector<std::string>	files;				// 複数あれば再生ごとにランダムに選ぶ
	SoundCategory				category = SoundCategory::SE;
	float						volume = 1.0f;
	float						pitchRandom = 0.0f;	// ピッチ揺らぎの幅(±)
	int							maxInstances = 0;	// 同時発音数の上限(0以下は無制限)
	bool						loop = false;
};

// 論理ID → SoundDef の対応表
class KdSoundDefTable
{
public:
	// JSONから読み込む(既存の定義は置き換える)
	bool Load(std::string_view path);

	// 未登録ならnullptr
	const SoundDef* Find(const std::string& soundId) const;

	void Clear() { m_defs.clear(); }

private:
	std::unordered_map<std::string, SoundDef> m_defs;
};
