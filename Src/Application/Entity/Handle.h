#pragma once
#include <cstdint>
#include <functional>
#include "HandleTable.h"

// 破棄されうる相手を安全に指すための非所有ハンドル(GameObject/ComponentBase共通)。
// 生存はHandleTable(スロット番号+世代)で判定し、参照先のメモリには触れない。
// TはHandleRootとGetHandleId()を持つ型。メンバ関数の中でだけT::HandleRootを使うので、Tは前方宣言のままでも宣言できる。
template <typename T>
class Handle {
public:
	Handle() = default;

	explicit Handle(T* obj) : id_(obj ? obj->GetHandleId() : 0) {}

	bool IsValid() const {
		return HandleTable<typename T::HandleRoot>::Get(id_) != nullptr;
	}

	T* Resolve() const {
		return static_cast<T*>(HandleTable<typename T::HandleRoot>::Get(id_));
	}

	bool operator==(const Handle& other) const { return id_ == other.id_; }
	bool operator!=(const Handle& other) const { return id_ != other.id_; }

	// ハッシュ計算用 アクセスには Resolve()を使う
	uint64_t GetId() const { return id_; }

private:
	uint64_t id_ = 0;
};

namespace std {
	template <typename T>
	struct hash<Handle<T>> {
		size_t operator()(const Handle<T>& handle) const {
			return std::hash<uint64_t>{}(handle.GetId());
		}
	};
}
