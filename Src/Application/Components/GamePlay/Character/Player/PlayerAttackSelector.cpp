#include "PlayerAttackSelector.h"

namespace
{
	// --- デバッグ用: 6段の通常コンボ ---------------------------------
	// 以前の「配列＋comboIndex_をインクリメント」と同じ並び順・同じ段数
	// (kMaxComboChainLength)を、ID参照のコンボ木として組み直したもの。
	// Attack1→Attack2→…→Attack6と一直線に繋がり、Attack6は
	// comboLinksが空(=コンボ終端)。
	//
	// 【TODO】実際の数値(ダメージ・秒数・アニメーション名等)は仮値。
	// 本来はCSV/JSON等の外部データから読み込む想定(EnemyDefinition等と
	// 同じ方針)だが、その読み込み経路が未整備なため、ひとまずコード内に
	// デバッグ用の値を直接書いている。
	PlayerAttackDefinition MakeComboHit(const std::string& id, const std::string& nextId,
		float windup, float active, float recovery, float damage)
	{
		PlayerAttackDefinition def;
		def.id = id;
		def.attack.info.damage = damage;
		def.attack.phaseData.animationName = id; // 仮: idをそのままクリップ名に流用
		def.attack.phaseData.windup.targetDuration = windup;
		def.attack.phaseData.active.targetDuration = active;
		def.attack.phaseData.recovery.targetDuration = recovery;
		def.attack.cancelData.comboWindowAfterRecovery = 0.2f;

		// Idleからは1段目(Attack1)にしか直接入れない。2段目以降は
		// 前段のcomboLinks経由でしか辿り着けない技として扱うため、
		// entryCommandを未設定(nullopt)にしておく。
		def.entryCommand = (id == "Attack1") ? std::optional<ActionCommand>(ActionCommand::Attack)
			: std::nullopt;

		if (!nextId.empty()) {
			ComboLink link;
			link.requiredCommand = ActionCommand::Attack;
			link.priority = 0;
			link.nextAttackId = nextId;
			def.comboLinks.push_back(link);
		}
		// nextIdが空(最終段)の場合はcomboLinksを空のままにする
		// →ここでコンボが終了する。

		return def;
	}

	PlayerAttackTable BuildDebugAttackTable()
	{
		PlayerAttackTable table;
		table.attacks.push_back(MakeComboHit("Attack1", "Attack2", 0.2f, 0.15f, 0.3f, 10.0f));
		table.attacks.push_back(MakeComboHit("Attack2", "Attack3", 0.2f, 0.15f, 0.3f, 10.0f));
		table.attacks.push_back(MakeComboHit("Attack3", "Attack4", 0.2f, 0.15f, 0.3f, 12.0f));
		table.attacks.push_back(MakeComboHit("Attack4", "Attack5", 0.25f, 0.15f, 0.35f, 12.0f));
		table.attacks.push_back(MakeComboHit("Attack5", "Attack6", 0.25f, 0.2f, 0.35f, 14.0f));
		table.attacks.push_back(MakeComboHit("Attack6", "", 0.3f, 0.2f, 0.5f, 20.0f)); // 最終段(フィニッシュ)
		return table;
	}
}

void PlayerAttackSelector::Awake()
{
	attackTable_ = BuildDebugAttackTable();

	// id重複・comboLinksのリンク先不在をここで検出する。テーブル定義側の
	// ミスはここで気付きたいため、実行時assertで止める
	// (PlayerAttackTable::Validate()参照)。
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
		// comboLinksに該当が無い場合、このコマンドではコンボを継続できない
		// (例: Attack入力しか繋がっていない技のRecovery中にEvadeが来た等)。
		// Idleからの新規開始(entryCommand)側へフォールスルーする。
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
	// 新しい攻撃を実際に開始した時点で、コンボ継続受付ウィンドウは
	// もう不要(TryResolveNext側で既に0にしているが、念のため明示する)。
	comboWindowRemaining_ = 0.0f;
}

void PlayerAttackSelector::NotifyRecoveryFinishedNaturally(float comboWindowAfterRecovery)
{
	comboWindowRemaining_ = comboWindowAfterRecovery;
	if (comboWindowRemaining_ <= 0.0f) {
		// ウィンドウが無い(0以下)技は、Recoveryが終わった瞬間に
		// コンボが打ち切られる。
		ResetCombo();
	}
}

void PlayerAttackSelector::ResetCombo()
{
	currentAttack_ = PlayerAttackDefinition{};
	comboWindowRemaining_ = 0.0f;
}
