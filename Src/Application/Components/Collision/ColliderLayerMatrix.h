#pragma once

#include "ColliderCategory.h"

// ============================================================
// レイヤー(カテゴリ)ごとの「デフォルトでどのカテゴリと判定するか」を
// 1箇所で管理するテーブル。
//
// Unity(Project Settings > Physics のレイヤー衝突マトリクス)や
// Unreal(Collision Presets)が持っている「プロジェクト全体で衝突関係を
// 一元管理する」仕組みに相当する。
//
// これが無いと、新しい形状を追加するたびに毎回collideMaskを手で
// 指定する必要があり、指定漏れ(本来ぶつかってほしいのに設定を忘れる/
// 逆に無関係なレイヤーと誤って判定してしまう)が起きやすい。
//
// 使い方:
//   起動時(シーン初期化など)に1回だけ、プロジェクト全体の方針として
//   デフォルト値を設定しておく。
//     ColliderLayerMatrix::Instance().SetDefaultMask(
//         ColliderCategory::HurtBox, ColliderCategory::HitBox);
//
//   ColliderComponent::AddSphere等でcollideMaskを省略すると、
//   (ColliderCategory::UseMatrixDefaultという特別な値がデフォルト引数に
//    なっているため)このテーブルから自動的に引かれる。
//   明示的にcollideMaskを指定すればテーブルより優先される
//   (Box2Dの「categoryBits/maskBitsを形状ごとに直接指定できる」
//    柔軟性は失わない、あくまで「省略時のデフォルト」を提供するだけの
//    仕組みという位置付け)。
//
// 未設定のカテゴリはAll(全レイヤーと判定)がフォールバックになる
// (「設定を書き忘れたら何とも判定しなくなって攻撃が素通りする」より、
//  「書き忘れたら以前と同じ全判定のまま気づける」方が安全なため)。
// ============================================================
class ColliderLayerMatrix
{
public:
	static ColliderLayerMatrix& Instance() {
		static ColliderLayerMatrix instance;
		return instance;
	}

	// categoryのデフォルトcollideMaskを設定する。
	// 双方向にしたい関係(HitBoxとHurtBoxが互いに判定したい、等)は
	// 呼び出し側で両方向分呼ぶこと(意図的に自動対称化はしていない。
	// 「AはBを見るがBはAを気にしない」という非対称な関係も表現できる
	// ようにするため。これはCollisionShapeEntry::CanCollideWithが
	// 元々持っている双方向チェックの設計とも一貫する)。
	void SetDefaultMask(ColliderCategory category, ColliderCategory mask) {
		defaults_[category] = mask;
	}

	ColliderCategory GetDefaultMask(ColliderCategory category) const {
		const auto it = defaults_.find(category);
		return (it != defaults_.end()) ? it->second : ColliderCategory::All;
	}

	// テスト/シーン切り替え時などに全設定をリセットしたい場合に使う。
	void Clear() { defaults_.clear(); }

private:
	ColliderLayerMatrix() = default;

	std::unordered_map<ColliderCategory, ColliderCategory, ColliderCategoryHash> defaults_;
};
