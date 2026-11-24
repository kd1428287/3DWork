#pragma once

// 破棄されうる相手を安全に指すための非所有ハンドル(GameObject/ComponentBase共通)
template <typename T>
class Handle {
public:
	Handle() = default;

	explicit Handle(T* obj)
		: ptr_(obj), generation_(obj ? obj->GetGeneration() : 0) {}

	// 同じポインタでも違うものだと判別するために、世代番号でも判定
	bool IsValid() const {
		return ptr_ != nullptr && ptr_->GetGeneration() == generation_;
	}

	T* Resolve() const {
		return IsValid() ? ptr_ : nullptr;
	}

	bool operator==(const Handle& other) const {
		return ptr_ == other.ptr_ && generation_ == other.generation_;
	}
	bool operator!=(const Handle& other) const {
		return !(*this == other);
	}

	// ハッシュ計算用 アクセスには Resolve()を使う
	T* GetRawPointerUnsafe() const { return ptr_; }
	uint64_t GetGenerationValue() const { return generation_; }

private:
	T* ptr_ = nullptr;
	uint64_t generation_ = 0;
};

namespace std {
	template <typename T>
	struct hash<Handle<T>> {
		size_t operator()(const Handle<T>& handle) const {
			// ポインタが同じでも世代が違えば異なるハッシュになる
			const size_t h1 = std::hash<void*>{}(handle.GetRawPointerUnsafe());
			const size_t h2 = std::hash<uint64_t>{}(handle.GetGenerationValue());
			return h1 ^ (h2 << 1);
		}
	};
}