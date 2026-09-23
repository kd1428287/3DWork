#include "FootDustComponent.h"

void FootDustComponent::Awake()
{
	// FootstepEventは同一GameObject(プレイヤー本体)のLocalEventBusに流れてくる想定。
	onFootSub_ = { &GetOwner()->GetLocalEventBus(),GetOwner()->GetLocalEventBus().Subscribe<FootstepEvent>(
		[this](const FootstepEvent& event) { OnFootstep(event); }) };
}

void FootDustComponent::OnFootstep(const FootstepEvent& event)
{
	BoneSocketComponent* left = nullptr;
	BoneSocketComponent* right = nullptr;

	if (footSocketLeft_.IsValid())left = footSocketLeft_.Resolve();
	if (footSocketRight_.IsValid())right = footSocketRight_.Resolve();

	const BoneSocketComponent* socket = (event.foot == FootSide::Left) ? left : right;
	if (socket == nullptr) return;

	// Directionは使わないため未指定({0,0,0})のまま渡す
	// (EffectEvents.hのコメント通り、方向を使わないエフェクトではこれでよい)。
	PublishGenericEffect(*GetOwner()->GetContext()->eventBus, effectId_, socket->GetWorldMatrix().Translation());
}