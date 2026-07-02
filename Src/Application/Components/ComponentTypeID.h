#pragma once

// ============================================================
// コンポーネントの型ごとに一意なIDを発行する仕組み。
// typeid(RTTI)を使わず、テンプレートの静的ローカル変数を利用して
// コンパイル時に近い形で型ごとの連番IDを割り当てる。
// ============================================================
using ComponentTypeId = std::size_t;

namespace detail {
	inline ComponentTypeId NextComponentTypeId() {
		static ComponentTypeId counter = 0;
		return counter++;
	}
}  // namespace detail

// T ごとに一意なIDを返す。初回呼び出し時に採番され、以降は同じ値を返す。
template <typename T>
ComponentTypeId GetComponentTypeId() {
	static const ComponentTypeId id = detail::NextComponentTypeId();
	return id;
}