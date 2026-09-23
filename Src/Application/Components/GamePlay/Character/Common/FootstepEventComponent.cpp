#include "FootstepEventComponent.h"

void FootstepEventComponent::Update(float /*deltaTime*/)
{
	if (animator_ == nullptr) return;

	const std::string_view currentClip = animator_->GetCurrentAnimationName();

	// クリップが切り替わった直後は、直前クリップのphaseとの比較が無意味なため
	// 現在位置に合わせるだけで今回の判定はスキップする(誤検知防止)。
	if (!hasLastClip_ || currentClip != lastClipName_) {
		lastClipName_ = std::string(currentClip);
		lastPhase_ = animator_->GetNormalizedTime();
		hasLastClip_ = true;
		return;
	}

	const float currentPhase = animator_->GetNormalizedTime();
	const auto* triggers = FindTriggersForClip(currentClip);

	if (triggers != nullptr) {
		for (const auto& trigger : *triggers) {
			if (CrossedPhase(lastPhase_, currentPhase, trigger.phase)) {
				PublishFootstepEvent(GetOwner()->GetLocalEventBus(), GetOwner(), trigger.foot);
			}
		}
	}

	lastPhase_ = currentPhase;
}

bool FootstepEventComponent::CrossedPhase(float lastPhase, float currentPhase, float triggerPhase)
{
	// 通常再生(巻き戻りなし)ならlastPhase < triggerPhase <= currentPhaseで判定。
	if (currentPhase >= lastPhase) {
		return lastPhase < triggerPhase && triggerPhase <= currentPhase;
	}
	// ループで1.0→0.0へ折り返した場合は[lastPhase,1.0)と[0.0,currentPhase]の両方を見る。
	return triggerPhase > lastPhase || triggerPhase <= currentPhase;
}

const std::vector<FootstepTrigger>* FootstepEventComponent::FindTriggersForClip(std::string_view clipName) const
{
	const auto it = triggerTable_.find(std::string(clipName));
	return (it != triggerTable_.end()) ? &it->second : nullptr;
}