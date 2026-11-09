#pragma once

#include "Application/Core/EventBus/Events/CombatEvents.h"

class RethalSyncronizationSystem {
public:
	RethalSyncronizationSystem() {};

	void SyncroStart(Handle<GameObject> atk, Handle<GameObject> vic);
	void Update(float dt);

private:

	Handle<GameObject> atk_;
	Handle<GameObject> vic_;

	Math::Vector3 syncroPoint_;
	
};