#pragma once

class ComponentRegistry;

// カテゴリ別の登録関数。RegisterAllComponents()(ComponentRegistrations.cpp)からだけ呼ぶ。
void RegisterGraphicsComponents(ComponentRegistry& registry);
void RegisterPhysicsComponents(ComponentRegistry& registry);
void RegisterCombatComponents(ComponentRegistry& registry);
void RegisterPlayerComponents(ComponentRegistry& registry);
void RegisterEnemyComponents(ComponentRegistry& registry);
void RegisterUIComponents(ComponentRegistry& registry);
void RegisterMapComponents(ComponentRegistry& registry);
