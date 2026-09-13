#pragma once

// ============================================================
// 当たり判定の「カテゴリ(何者であるか)」を表すビットフラグ。
// KdCollider::Typeの役割を引き継ぐが、名前はこのプロジェクトの
// 命名に合わせて付け直している。
//
// 以前は ColliderLayer という素の uint32_t 定数の集まり(namespace)
// だったが、以下の問題があった:
//   - 型安全性が無い。entry.layer = 42 のような無意味な生数値や、
//     全く無関係な別のビットマスクとの混同がコンパイラで検出できない。
//   - 「layer」という名前が「1つの物は1つの層に属する」という
//     単数のニュアンスを連想させるが、実際には1つの形状が複数の
//     カテゴリを同時に名乗ってよい設計(例: 坂道の地形が
//     Ground|Bumpを同時に持つ)だったため、名前と実態が食い違っていた。
//
// enum class化して型安全性を確保しつつ、名前もBox2Dの
// categoryBits/maskBitsの発想を借りて「合成可能な集合である」ことが
// 伝わるものに変更した(CollisionShapeEntry::categoryMask/collideMask
// 参照)。
// ============================================================
enum class ColliderCategory : uint32_t
{
	None = 0,
	Ground = 1u << 0, // 地形。上に乗れる/地面判定に使う
	Bump = 1u << 1, // 横方向の押し合い(壁・キャラ同士等、物理的に実体のあるもの)
	HitBox = 1u << 2, // 攻撃判定(形状ベース)
	HitLine = 1u << 3, // 攻撃判定(レイベース。RaycastSystem向け)
	HurtBox = 1u << 4, // 食らい判定
	Sight = 1u << 5, // 視界判定
	EventArea = 1u << 6, // イベント用の任意領域
	All = 0xFFFFFFFFu,

	// Add*系のcollideMask引数を省略した時の目印専用の値。
	// 実際の判定には使わず、AddOrReplace内でColliderLayerMatrixの
	// デフォルト値に解決してから形状に保存する(形状自身がこの値を
	// 持ったまま残ることは無い)。All(0xFFFFFFFF)とビットが被らないよう
	// にする必要はない(そもそも解決前に置き換わるため)が、
	// 誤って直接指定されても分かるよう別名として定義しておく。
	UseMatrixDefault = 0xFFFFFFFEu,
};

// ビット演算だけを許可する(算術演算・他の型との暗黙変換はできないまま)。
// 「合成可能な集合」という実態を、型を通じても表現するための最小限の
// 演算子オーバーロード。
constexpr ColliderCategory operator|(ColliderCategory a, ColliderCategory b) {
	return static_cast<ColliderCategory>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr ColliderCategory operator&(ColliderCategory a, ColliderCategory b) {
	return static_cast<ColliderCategory>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
constexpr ColliderCategory& operator|=(ColliderCategory& a, ColliderCategory b) { a = a | b; return a; }
constexpr ColliderCategory& operator&=(ColliderCategory& a, ColliderCategory b) { a = a & b; return a; }

// 1ビットでも立っていればtrue(「重なりがあるか」の判定に使う)。
constexpr bool Any(ColliderCategory v) { return static_cast<uint32_t>(v) != 0; }

// 立っているビットがちょうど1つか(categoryMask側の誤用検出に使う。
// collideMask側は複数ビットのORが正常なので、こちらには使わないこと)。
constexpr bool IsSingleCategory(ColliderCategory v) {
	const uint32_t bits = static_cast<uint32_t>(v);
	return bits != 0 && (bits & (bits - 1)) == 0;
}

// std::unordered_map等で ColliderCategory をキーに使うためのハッシュ。
// enum classはデフォルトでstd::hashに対応していないため明示的に用意する。
struct ColliderCategoryHash {
	size_t operator()(ColliderCategory v) const {
		return std::hash<uint32_t>{}(static_cast<uint32_t>(v));
	}
};
