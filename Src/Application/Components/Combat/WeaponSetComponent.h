#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "WeaponComponent.h"
#include "../../Core/Handle.h"

// ============================================================
// WeaponSetComponent
// キャラクター(Player/Enemy)1体が持つ「武器/攻撃部位」の集合を、
// スロット名(文字列)で管理する汎用コンポーネント。
//
// Player: 装備の付け替え等は今は無く、常に単一スロット
//         (PlayerStatusController::kMainWeaponSlot = "Main")のみ使用する。
// Enemy : 素手キャラの拳・足・尻尾など、部位ごとに複数登録できる
//         (例: "LeftFist"/"RightFist"/"RightFoot"等)。技データ側
//         (AttackMoveData::weaponSlots / EnemyAttackDefinition::weaponSlots)
//         が「どのスロットを有効化するか」を指定する。
//
// スロット名をenumではなく文字列にしているのは、EnemyAIDataが
// 「C++の型を分けずデータの中身の差し替えで敵種の個体差を表現する」
// というデータ駆動方針を取っているため(EnemyAIData.h冒頭コメント参照)。
// 敵種を追加するたびに部位名のenumを増やす必要が無いようにしている。
// ============================================================
class WeaponSetComponent : public ComponentBase
{
public:
	explicit WeaponSetComponent(GameObject* owner) : ComponentBase(owner) {}

	// 装備/セットアップ時に呼ぶ。武器(または部位)GameObject側の
	// WeaponComponentを、キャラクター側のスロット名に紐付ける。
	void RegisterWeapon(const std::string& slot, Handle<WeaponComponent> weapon)
	{
		weapons_[slot] = weapon;
	}

	void SetHitBoxEnabled(const std::vector<std::string>& slots, bool enabled)
	{
		for (const auto& slot : slots) {
			if (WeaponComponent* weapon = Resolve(slot)) {
				weapon->SetHitBoxEnabled(enabled);
			}
		}
	}

	void SetTrailEmitting(const std::vector<std::string>& slots, bool emitting)
	{
		for (const auto& slot : slots) {
			if (WeaponComponent* weapon = Resolve(slot)) {
				weapon->SetTrailEmitting(emitting);
			}
		}
	}

	WeaponComponent* GetWeapon(const std::string& slot) const { return Resolve(slot); }

private:
	WeaponComponent* Resolve(const std::string& slot) const
	{
		auto it = weapons_.find(slot);
		return (it != weapons_.end()) ? it->second.Resolve() : nullptr;
	}

	std::unordered_map<std::string, Handle<WeaponComponent>> weapons_;
};