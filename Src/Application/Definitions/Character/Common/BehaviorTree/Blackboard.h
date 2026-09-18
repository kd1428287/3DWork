#pragma once
#include <variant>
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

// ============================================================
// Blackboard
//
// BT/AIノードが「特定のContextの型(EnemyAIController等)を直接知らずに」
// 名前(キー)経由で値を読み書きするための、型消去されたキー・バリュー
// ストア。BT制御エディタでノードグラフからConditionノードを組む際、
// ノード自身はC++の型やメンバ関数を一切知らず、「このキーの値がこの
// 条件を満たすか」だけを見ればよくなる。これがノードグラフ上で
// パラメータ(キー名・比較値)として設定できるようになるための前提。
//
// 【値の計算とBlackboardへの書き込みは別責務】
// Blackboardは値の"置き場"でしかない。値そのものの計算はこれまで通り
// C++側(EnemyAIController::Update()等)が担う。「HasTarget()の判定
// ロジックそのものをデータ化する」わけではなく、「その判定結果をBTの
// ノードが読めるように公開する」ことだけがこのクラスの役割。
//
// 【値の性質は2種類ある】
// - 毎フレーム上書きされる「知覚」値(例: DistanceToTarget、HasTarget。
//   EnemyAIController::Update()が毎フレーム書き込み直す)。
// - 誰かが書いてから、別の誰かが消費するまで残り続ける「要求/状態」値
//   (例: 今どの割り込み演出が要求されているか)。
// どちらも同じ1つのフラットなキー・バリューストアで表現でき、
// 「いつ書くか/いつ消すか」は読み書きする側(EnemyAIController、
// 各IEnemyBehavior実装等)のポリシーに委ねる。Blackboard自身は
// 値の寿命管理(自動失効等)を持たない。
//
// 【対応する値の型について】
// 現状のBT側の要件(距離・フラグ・アニメーション名程度)を満たす最小限の
// 型だけをVariantに含める。将来Vector3やGameObjectハンドルが必要に
// なったら、その時点でVariantへ型を足す(先回りして型を増やしすぎない)。
//
// 【キーの型をstd::stringにしている理由】
// ノードグラフエディタ上でユーザーが自由にキー名を入力/選択できる
// ことを優先し、intern済みID等の最適化は行わない。敵1体あたりの
// キー数・Tick頻度を考えると、文字列比較のコストが問題になる場面は
// 想定しにくい。もし将来プロファイルして問題になった場合は、
// キーをコンパイル時ハッシュ(あるいは登録済みIDへのintern)へ
// 置き換えることを検討する。
// ============================================================
class Blackboard
{
public:
	// キーが存在するが値の型が要求と異なる場合も「無い」扱い(nullopt)にする。
	// 【要検討】現状は静かにnulloptを返すだけなので、エディタ側で
	// キーの型を取り違えて設定した場合、実行時にConditionが常にFailureを
	// 返すだけで原因に気付きにくい。デバッグビルドでは型不一致を検出した
	// 時点で一度だけログを出す、といった診断機構を後で足すことを検討する。
	void SetBool(const std::string& key, bool value) { values_[key] = value; }
	void SetFloat(const std::string& key, float value) { values_[key] = value; }
	void SetString(const std::string& key, std::string value) { values_[key] = std::move(value); }

	std::optional<bool> GetBool(const std::string& key) const { return Get<bool>(key); }
	std::optional<float> GetFloat(const std::string& key) const { return Get<float>(key); }
	std::optional<std::string> GetString(const std::string& key) const { return Get<std::string>(key); }

	// 値が無い(または型が違う)場合にdefaultValueを返す簡易版。
	// Conditionノード側で「キーが未設定なら安全なデフォルトとして扱う」
	// ような書き方をしたい場合に使う。
	bool GetBoolOr(const std::string& key, bool defaultValue) const { return GetBool(key).value_or(defaultValue); }
	float GetFloatOr(const std::string& key, float defaultValue) const { return GetFloat(key).value_or(defaultValue); }
	std::string GetStringOr(const std::string& key, std::string defaultValue) const {
		auto value = GetString(key);
		return value.has_value() ? *value : std::move(defaultValue);
	}

	bool HasKey(const std::string& key) const { return values_.find(key) != values_.end(); }

	// 「要求/状態」値を消費し終えた際に、キー自体を取り除く
	// (空文字列/falseで上書きするのではなく、エントリ自体を消す。
	// HasKey()で「一度でも設定されたことがあるか」を正しく判定できる
	// ようにするため)。
	void ClearKey(const std::string& key) { values_.erase(key); }

	void Clear() { values_.clear(); }

	// エディタのキー選択UI(ドロップダウン等)向けに、現在設定されている
	// キー名の一覧を返す。デバッグ表示にも流用できる。
	std::vector<std::string> ListKeys() const {
		std::vector<std::string> keys;
		keys.reserve(values_.size());
		for (const auto& [key, value] : values_) keys.push_back(key);
		return keys;
	}

private:
	// std::anyではなくstd::variantにしているのは、現状扱う型が
	// bool/float/std::stringの3つに限定されており、型安全な
	// std::get_ifで値を取り出せる方を優先したため
	// (std::anyはany_castの型を呼び出し側が完全に正しく知っている
	// 前提が必要で、Blackboardのような「キーから引く」用途では
	// variantの方が事故りにくい)。
	using Value = std::variant<std::monostate, bool, float, std::string>;

	template <typename T>
	std::optional<T> Get(const std::string& key) const
	{
		const auto it = values_.find(key);
		if (it == values_.end()) return std::nullopt;
		if (const T* value = std::get_if<T>(&it->second)) return *value;
		return std::nullopt; // キーは存在するが型が違う場合も「無い」扱いにする
	}

	std::unordered_map<std::string, Value> values_;
};
