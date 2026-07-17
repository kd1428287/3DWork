#pragma once

#include "GameObject.h"

// ============================================================
// 任意の型T(GameObject、あるいはComponentBase派生型)への
// 「安全な」非所有参照。
//
// 生ポインタ(T*)を、フレームをまたいで/生存期間が結びついていない
// 相手への参照として保持し続けると、以下のようなすり替わり事故
// (いわゆるABA問題)が起こり得る:
//   1. 対象Aへのポインタを保持する
//   2. Aが破棄される(ヒープ領域が解放される)
//   3. 同じフレーム内で、別の新しいインスタンスBが同じヒープ領域に
//      確保される
//   4. 保持していたポインタは「Aが破棄された」ことに気づけないまま、
//      Bを指すポインタとして扱われてしまう
//
// Handle<T>は、ポインタに加えて生成時点の世代番号(T::GetGeneration())を
// 組で保持し、IsValid()で「ptrの指す先が、本当に自分が参照したかった
// インスタンスと同一か」を確認できるようにする。
//
// Tに要求する条件はGetGeneration()を持つことだけ(ダックタイピング。
// GameObject/ComponentBaseはどちらも独立にこれを実装しており、
// 共通の基底クラスを持たないためテンプレートで型を横断している)。
//
// 使い分け:
//   - 同じGameObject上の兄弟コンポーネントへの参照(Start()で
//     GetComponent<T>()して保持するもの)には不要。生存期間が
//     結びついているため、今まで通り生ポインタで十分。
//   - フレームをまたいで、かつ生存期間の結びつきが無い相手を
//     指し続けたい場合(CollisionSystemの重なりペア記録、
//     AIのターゲティング等)にはこちらを使う。
//
// 以前はGameObject用(GameObjectHandle)とコンポーネント用
// (ComponentHandle<T>)を別々のクラスとして持っていたが、中身が
// 完全に同じロジックの重複だったため、この1つのテンプレートに統合した。
// 呼び出し側の互換性のため、旧名は下部でエイリアスとして残している。
// ============================================================
template <typename T>
class Handle {
public:
	Handle() = default;

	explicit Handle(T* obj)
		: ptr_(obj), generation_(obj ? obj->GetGeneration() : 0) {}

	// 参照先が破棄されておらず、かつ生成時と同じ世代のインスタンスを
	// 指していればtrue。破棄後に別のインスタンスへポインタ値が
	// 再利用されていた場合は世代が一致しないためfalseになる。
	bool IsValid() const {
		return ptr_ != nullptr && ptr_->GetGeneration() == generation_;
	}

	// 有効な場合のみポインタを返す。無効ならnullptr。
	// 「無効かもしれないポインタを、確認せずそのまま使ってしまう」事故を
	// 避けるため、検証をすり抜けられる生ポインタ用Getterは用意していない。
	T* Resolve() const {
		return IsValid() ? ptr_ : nullptr;
	}

	bool operator==(const Handle& other) const {
		return ptr_ == other.ptr_ && generation_ == other.generation_;
	}
	bool operator!=(const Handle& other) const {
		return !(*this == other);
	}

	// ハッシュ計算用。「同じアドレス値を指しているか」の比較であって
	// 「同じインスタンスを指しているか」の判定ではないため、参照先へ
	// アクセスする用途では使わないこと(必ずResolve()/IsValid()を経由する)。
	T* GetRawPointerUnsafe() const { return ptr_; }
	uint64_t GetGenerationValue() const { return generation_; }

private:
	T* ptr_ = nullptr;
	uint64_t generation_ = 0;
};

// unordered_map/unordered_set等のキーとして使えるようにする
namespace std {
	template <typename T>
	struct hash<Handle<T>> {
		size_t operator()(const Handle<T>& handle) const {
			// ptr_だけでなくgeneration_も混ぜる。
			// (ptr_が破棄後に再利用されても、世代が違えば別のハッシュ値になる)
			const size_t h1 = std::hash<void*>{}(handle.GetRawPointerUnsafe());
			const size_t h2 = std::hash<uint64_t>{}(handle.GetGenerationValue());
			return h1 ^ (h2 << 1);
		}
	};
}