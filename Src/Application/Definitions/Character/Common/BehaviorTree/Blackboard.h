#pragma once
#include <variant>
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

// BTノードがControllerの型を知らずに、キー名で値を読み書きするための型消去キーバリューストア。
// 値の計算はC++側(EnemyAIController等)が行い、Blackboardは結果の置き場に徹する。
// 値には毎フレーム上書きされる知覚値(HasTarget等)と、消費されるまで残る要求値(割り込み要求等)があり、
// 書く/消すタイミングは利用側に任せる(自動失効は持たない)。
// 対応型は必要最小限(bool/float/string)。キーはエディタで自由に入力できるようstd::stringにしている。
class Blackboard
{
public:
	// キーはあっても型が違う場合は「無い」扱い(nullopt)。
	// 【要検討】型の取り違えに気づけないため、デバッグビルドで警告を出す仕組みを足したい。
	void SetBool(const std::string& key, bool value) { values_[key] = value; }
	void SetFloat(const std::string& key, float value) { values_[key] = value; }
	void SetString(const std::string& key, std::string value) { values_[key] = std::move(value); }

	std::optional<bool> GetBool(const std::string& key) const { return Get<bool>(key); }
	std::optional<float> GetFloat(const std::string& key) const { return Get<float>(key); }
	std::optional<std::string> GetString(const std::string& key) const { return Get<std::string>(key); }

	// 値が無い(または型が違う)場合にdefaultValueを返す。
	bool GetBoolOr(const std::string& key, bool defaultValue) const { return GetBool(key).value_or(defaultValue); }
	float GetFloatOr(const std::string& key, float defaultValue) const { return GetFloat(key).value_or(defaultValue); }
	std::string GetStringOr(const std::string& key, std::string defaultValue) const {
		auto value = GetString(key);
		return value.has_value() ? *value : std::move(defaultValue);
	}

	bool HasKey(const std::string& key) const { return values_.find(key) != values_.end(); }

	// 消費済みの要求値をエントリごと消す(HasKey()で「設定済みか」を判定できるようにするため)。
	void ClearKey(const std::string& key) { values_.erase(key); }

	void Clear() { values_.clear(); }

	// エディタのキー選択UIやデバッグ表示向けに、設定済みのキー名を返す。
	std::vector<std::string> ListKeys() const {
		std::vector<std::string> keys;
		keys.reserve(values_.size());
		for (const auto& [key, value] : values_) keys.push_back(key);
		return keys;
	}

private:
	// 型安全にstd::get_ifで取り出せるため、anyではなくvariantを使う
	using Value = std::variant<std::monostate, bool, float, std::string>;

	template <typename T>
	std::optional<T> Get(const std::string& key) const
	{
		const auto it = values_.find(key);
		if (it == values_.end()) return std::nullopt;
		if (const T* value = std::get_if<T>(&it->second)) return *value;
		return std::nullopt;	// 型違いも「無い」扱い
	}

	std::unordered_map<std::string, Value> values_;
};
