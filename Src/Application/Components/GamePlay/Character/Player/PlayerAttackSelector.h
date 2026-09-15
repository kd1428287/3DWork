#pragma once
#include "Application/Definitions/Character/Player/PlayerCombatTypes.h"

// ============================================================
// PlayerAttackSelector
//
// 「次にどの攻撃を出すか」の選択と、コンボ継続受付ウィンドウの管理を
// 担当する、PlayerStatusControllerの兄弟コンポーネント。
//
// PlayerAttackTable(ID参照のコンボ木、PlayerCombatTypes.h)を保持し、
// 入力コマンドから次のPlayerAttackDefinitionを解決する。旧
// 「配列＋comboIndex_をインクリメント」を、この専用コンポーネントへ
// 置き換える。
// ============================================================
class PlayerAttackSelector : public ComponentBase
{
public:
	explicit PlayerAttackSelector(GameObject* owner) : ComponentBase(owner) {}

	void Awake() override;
	void Update(float deltaTime) override;

	// Idle(現在いずれの技も選択されていない)、またはRecovery中のコンボ
	// 継続受付ウィンドウ内で、PlayerStatusController::TryStartAttack()から
	// 呼ばれる。commandに対応する技が見つかればtrueを返し、以後
	// GetCurrentAttackData()で取得できるようになる。見つからなければ
	// falseを返す(呼び出し側は何もしない=攻撃を開始しない)。
	bool TryResolveNext(ActionCommand command);

	// 現在選択されている技のAttackData。StateAttackが毎フレーム参照する。
	const AttackData& GetCurrentAttackData() const { return currentAttack_.attack; }

	// 新しい攻撃を実際に開始した時点(PlayerStatusController::
	// OnStateChanged、遷移先がStateAttackになった瞬間)で呼ばれる。
	void NotifyAttackStarted();

	// AttackのRecoveryが(中断されずに)自然終了してNoneへ戻った時点で
	// 呼ばれる。comboWindowAfterRecovery秒だけコンボ継続入力の受付
	// ウィンドウを開く(0以下ならその場でコンボを打ち切る)。
	void NotifyRecoveryFinishedNaturally(float comboWindowAfterRecovery);

	// Evade/Guard/Staggerへの割り込み等、コンボ継続を認めず打ち切る場合に
	// PlayerStatusController::OnStateChangedから呼ばれる。
	void ResetCombo();

private:
	// currentAttack_.comboLinksの中から、commandに一致しpriorityが
	// 最小(=最優先)のリンクを辿った先の技を返す。無ければnullptr。
	const PlayerAttackDefinition* ResolveViaComboLinks(ActionCommand command) const;

	// attackTable_全体から、entryCommandがcommandと一致する技を探す
	// (Idleからの新規コンボ開始用)。複数該当する場合は登録順で先頭を
	// 採用する。無ければnullptr。
	const PlayerAttackDefinition* ResolveViaEntryCommand(ActionCommand command) const;

	PlayerAttackTable attackTable_;

	// 現在選択されている技(コンボ継続受付ウィンドウ内は、Recovery終了時点の
	// 技を保持したままにする。次の技を辿るための基点として使うため)。
	PlayerAttackDefinition currentAttack_;

	// コンボ継続受付ウィンドウの残り秒数。0ならウィンドウは閉じている。
	float comboWindowRemaining_ = 0.0f;
};
