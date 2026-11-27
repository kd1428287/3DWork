#pragma once
#include <cassert>
#include <cstdint>

// 生存判定用のスロット表。Handleはここへのindexと世代だけを持ち、参照先のメモリには触れない。
// 単一スレッド前提(生成・破棄はメインスレッドのみ)。
template <typename Root>
class HandleTable {
public:
	// スロット数の上限。index 0は「無効なHandle」用に予約する
	static constexpr uint32_t kCapacity = 65536;

	// スロットを確保してidを返す。足りない場合は0(常に無効なid)を返す
	static uint64_t Allocate(Root* obj)
	{
		uint32_t index = freeHead_;
		if (index != 0) {
			freeHead_ = slots_[index].next;
		}
		else {
			if (highWater_ + 1 >= kCapacity) {
				assert(false && "HandleTable: スロットが足りません");
				return 0;
			}
			index = ++highWater_;
		}

		Slot& slot = slots_[index];
		if (slot.generation == 0) slot.generation = 1;
		slot.ptr = obj;
		return Pack(index, slot.generation);
	}

	// スロットを解放して世代を進める。以前のHandleはすべて無効になる
	static void Free(uint64_t id)
	{
		const uint32_t index = Index(id);
		if (index == 0) return;

		Slot& slot = slots_[index];
		assert(slot.ptr != nullptr && "HandleTable: 二重解放");
		slot.ptr = nullptr;
		if (++slot.generation == 0) slot.generation = 1;
		slot.next = freeHead_;
		freeHead_ = index;
	}

	// 生きていればポインタ、無効ならnullptr
	static Root* Get(uint64_t id)
	{
		const uint32_t index = Index(id);
		assert(index < kCapacity);
		const Slot& slot = slots_[index];
		return slot.generation == Generation(id) ? slot.ptr : nullptr;
	}

private:
	struct Slot {
		Root* ptr = nullptr;
		uint32_t generation = 0;
		uint32_t next = 0;	// 空きスロットの連結用
	};

	static constexpr uint64_t Pack(uint32_t index, uint32_t generation) { return (static_cast<uint64_t>(generation) << 32) | index; }
	static constexpr uint32_t Index(uint64_t id) { return static_cast<uint32_t>(id); }
	static constexpr uint32_t Generation(uint64_t id) { return static_cast<uint32_t>(id >> 32); }

	inline static Slot slots_[kCapacity];
	inline static uint32_t freeHead_ = 0;
	inline static uint32_t highWater_ = 0;
};
