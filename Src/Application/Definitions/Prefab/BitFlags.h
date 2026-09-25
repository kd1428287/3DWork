#pragma once

template <typename Enum>
class BitFlags {
public:
	using Underlying = std::underlying_type_t<Enum>;

	BitFlags() : mask_(0) {}
	BitFlags(Enum flag) : mask_(static_cast<Underlying>(flag)) {}

	void add(Enum flag) { mask_ |= static_cast<Underlying>(flag); }
	bool has(Enum flag) const { return (mask_ & static_cast<Underlying>(flag)) != 0; }

	Underlying raw() const { return mask_; }
	void set_raw(Underlying mask) { mask_ = mask; }

private:
	Underlying mask_;
};

// Enumに対応する文字列マッピング表を提供するTraits
template <typename Enum>
struct EnumFlagStringMap;

template <typename BasicJsonType, typename Enum>
inline void from_json(const BasicJsonType& j, BitFlags<Enum>& flags)
{
	flags.set_raw(0);

	for (const std::string& name : j.template get<std::vector<std::string>>()) {
		bool found = false;

		// entries を直接ループ
		for (const auto& entry : EnumFlagStringMap<Enum>::entries) {
			if (name == entry.first) {
				flags.add(entry.second);
				found = true;
				break;
			}
		}
		if (!found) {
			throw std::runtime_error("Unknown Enum flag name: " + name);
		}
	}
}

template <typename BasicJsonType, typename Enum>
inline void to_json(BasicJsonType& j, const BitFlags<Enum>& flags)
{
	j = BasicJsonType::array();

	// entries を直接ループ
	for (const auto& entry : EnumFlagStringMap<Enum>::entries) {
		if (flags.has(entry.second)) {
			j.push_back(entry.first);
		}
	}
}