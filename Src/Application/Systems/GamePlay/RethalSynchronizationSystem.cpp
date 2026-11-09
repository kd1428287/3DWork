#include "Pch.h"
#include "RethalSynchronizationSystem.h"

void RethalSyncronizationSystem::SyncroStart(Handle<GameObject> atk, Handle<GameObject> vic)
{
	atk_ = atk;
	vic_ = vic;

	Math::Vector3 atkPos = atk_.Resolve()->GetComponent<TransformComponent>()->GetPosition();
	Math::Vector3 vicPos = vic_.Resolve()->GetComponent<TransformComponent>()->GetPosition();



}

void RethalSyncronizationSystem::Update(float dt)
{}
