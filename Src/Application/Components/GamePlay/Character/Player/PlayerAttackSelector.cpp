#include "PlayerAttackSelector.h"

void PlayerAttackSelector::Awake()
{
	assert(attackTable_.Validate() && "PlayerAttackTable: id重複、またはcomboLinksの参照先不在があります");
}

void PlayerAttackSelector::Update(float deltaTime)
{
	if (comboWindowRemaining_ <= 0.0f) return;

	comboWindowRemaining_ -= deltaTime;
	if (comboWindowRemaining_ <= 0.0f) {
		ResetCombo();
	}
}

bool PlayerAttackSelector::TryResolveNext(ActionCommand command)
{
	// コンボ継続受付ウィンドウ内(currentAttack_が設定済み)なら、
	// まずcomboLinksから繋がる次の技を探す。
	if (!currentAttack_.id.empty()) {
		if (const PlayerAttackDefinition* next = ResolveViaComboLinks(command)) {
			currentAttack_ = *next;
			comboWindowRemaining_ = 0.0f;
			return true;
		}
	}

	const PlayerAttackDefinition* entry = ResolveViaEntryCommand(command);
	if (entry == nullptr) return false;

	currentAttack_ = *entry;
	comboWindowRemaining_ = 0.0f;
	return true;
}

const PlayerAttackDefinition* PlayerAttackSelector::ResolveViaComboLinks(ActionCommand command) const
{
	const ComboLink* best = nullptr;
	for (const ComboLink& link : currentAttack_.comboLinks) {
		if (link.requiredCommand != command) continue;
		if (best == nullptr || link.priority < best->priority) {
			best = &link;
		}
	}
	return (best != nullptr) ? attackTable_.Find(best->nextAttackId) : nullptr;
}

const PlayerAttackDefinition* PlayerAttackSelector::ResolveViaEntryCommand(ActionCommand command) const
{
	for (const PlayerAttackDefinition& attack : attackTable_.attacks) {
		if (attack.entryCommand.has_value() && attack.entryCommand.value() == command) {
			return &attack;
		}
	}
	return nullptr;
}

void PlayerAttackSelector::NotifyAttackStarted()
{
	comboWindowRemaining_ = 0.0f;
}

void PlayerAttackSelector::NotifyRecoveryFinishedNaturally(float comboWindowAfterRecovery)
{
	comboWindowRemaining_ = comboWindowAfterRecovery;
	if (comboWindowRemaining_ <= 0.0f) {
		ResetCombo();
	}
}

void PlayerAttackSelector::ResetCombo()
{
	currentAttack_ = PlayerAttackDefinition{};
	comboWindowRemaining_ = 0.0f;
}
