#pragma once
#include "WeaponComponent.h"

// キャラクターが持つ攻撃判定をスロット名で管理する汎用コンポーネント
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

	void SetAttackDamageData(const std::vector<std::string>& slots, AttackDamageData info)
	{
		for (const auto& slot : slots) {
			if (WeaponComponent* weapon = Resolve(slot)) {
				weapon->SetAttackDamageData(info);
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