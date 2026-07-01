#pragma once
#include <cstddef>

//=============================================================================
// ComponentTypeID
//
// コンポーネントの型ごとに一意なIDを払い出すユーティリティ。
// RTTIを使わず、テンプレートの静的変数の初期化順を利用して
// コンパイル時に近い形でIDを確定させる。
//
// 使い方:
//   size_t id = ComponentTypeID::Get<TransformComponent>();
//
// 同じ型に対しては常に同じIDが返る。
// Registryがコンポーネントのストレージを配列で管理する際の添字になる。
//=============================================================================
class ComponentTypeID
{
public:
	template<class T>
	static size_t Get()
	{
		// 型ごとに静的変数が1つ生成されるため、型ごとに異なるアドレスを持つ
		static const size_t id = s_counter++;
		return id;
	}

	static size_t Count() { return s_counter; }

private:
	inline static size_t s_counter = 0;
};