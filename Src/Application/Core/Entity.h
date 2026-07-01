#pragma once
#include <cstdint>
#include <limits>

//=============================================================================
// Entity
//
// ECSにおける「存在」の識別子。
// データも振る舞いも持たず、ただのIDである。
//
// 【構造】
//  32bitのうち上位8bitを「世代(Generation)」、下位24bitを「インデックス」として使う。
//
//  世代を持つ理由:
//    インデックスは削除後に再利用される。
//    世代が異なれば「同じインデックスだが別のEntity」と判別できる。
//    これにより「削除済みEntityへの参照」を検出できる。
//=============================================================================

using EntityIndex = uint32_t;
using EntityGen = uint8_t;

struct Entity
{
	static constexpr uint32_t INDEX_BITS = 24;
	static constexpr uint32_t GEN_BITS = 8;
	static constexpr uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1;
	static constexpr uint32_t GEN_SHIFT = INDEX_BITS;
	static constexpr uint32_t INVALID_ID = std::numeric_limits<uint32_t>::max();

	uint32_t id = INVALID_ID;

	Entity() = default;
	explicit Entity(EntityIndex index, EntityGen gen)
		: id((static_cast<uint32_t>(gen) << GEN_SHIFT) | (index & INDEX_MASK))
	{}

	EntityIndex GetIndex() const { return id & INDEX_MASK; }
	EntityGen   GetGen()   const { return static_cast<EntityGen>(id >> GEN_SHIFT); }

	bool IsValid() const { return id != INVALID_ID; }

	bool operator==(const Entity& other) const { return id == other.id; }
	bool operator!=(const Entity& other) const { return id != other.id; }
};