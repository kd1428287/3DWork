#pragma once

#include <stdexcept>

#include "nlohmann/json.hpp"

// Vector3は[x, y, z]、Quaternionは[x, y, z, w]の配列で表す。要素数が違えば例外にする。
// BasicJsonTypeでテンプレート化し、ordered_jsonからも呼べるようにしている。
namespace nlohmann
{
	template<>
	struct adl_serializer<Math::Vector3>
	{
		template <typename BasicJsonType>
		static void from_json(const BasicJsonType& j, Math::Vector3& v)
		{
			if (!j.is_array() || j.size() != 3) throw std::runtime_error("Vector3 must be [x, y, z]");
			v = Math::Vector3(j[0].template get<float>(), j[1].template get<float>(), j[2].template get<float>());
		}

		template <typename BasicJsonType>
		static void to_json(BasicJsonType& j, const Math::Vector3& v) { j = BasicJsonType::array({ v.x, v.y, v.z }); }
	};

	// Quaternionは[x, y, z, w]の配列で表す。要素数が4でなければ例外にする。
	template<>
	struct adl_serializer<Math::Quaternion>
	{
		template <typename BasicJsonType>
		static void from_json(const BasicJsonType& j, Math::Quaternion& q)
		{
			if (!j.is_array() || j.size() != 4) throw std::runtime_error("Quaternion must be [x, y, z, w]");
			q = Math::Quaternion(j[0].template get<float>(), j[1].template get<float>(), j[2].template get<float>(), j[3].template get<float>());
		}

		template <typename BasicJsonType>
		static void to_json(BasicJsonType& j, const Math::Quaternion& q) { j = BasicJsonType::array({ q.x, q.y, q.z, q.w }); }
	};
}
