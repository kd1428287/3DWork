//#include "PlayerCombatDataTable.h"
//
//namespace
//{
//	// --- デバッグ用: 6段の通常コンボ ---------------------------------
//	// 以前の「配列＋comboIndex_をインクリメント」と同じ並び順・同じ段数
//	// (kMaxComboChainLength)を、ID参照のコンボ木として組み直したもの。
//	// Attack1→Attack2→…→Attack6と一直線に繋がり、Attack6は
//	// comboLinksが空(=コンボ終端)。
//	//
//	// 【TODO】実際の数値(ダメージ・秒数・アニメーション名等)は仮値。
//	// 本来はCSV/JSON等の外部データから読み込む想定(EnemyDefinition等と
//	// 同じ方針)だが、その読み込み経路が未整備なため、ひとまずコード内に
//	// デバッグ用の値を直接書いている。
//	// (旧PlayerAttackSelector.cppから移設。PlayerAttackSelector自身は
//	//  データを「持たされる」側に徹し、直書きデータの置き場所ではなく
//	//  なったため。)
//	PlayerAttackDefinition MakeComboHit(const std::string& id, const std::string& nextId,
//		float windup, float active, float recovery, float damage)
//	{
//		PlayerAttackDefinition def;
//		def.id = id;
//		def.attack.damageData.damage = damage;
//		def.attack.phaseData.animationName = id; // 仮: idをそのままクリップ名に流用
//		def.attack.phaseData.windup.targetDuration = windup;
//		def.attack.phaseData.active.targetDuration = active;
//		def.attack.phaseData.recovery.targetDuration = recovery;
//		def.attack.cancelData.comboWindowAfterRecovery = 0.2f;
//
//		// Idleからは1段目(Attack1)にしか直接入れない。2段目以降は
//		// 前段のcomboLinks経由でしか辿り着けない技として扱うため、
//		// entryCommandを未設定(nullopt)にしておく。
//		def.entryCommand = (id == "APose_Attack02_1") ? std::optional<ActionCommand>(ActionCommand::Attack)
//			: std::nullopt;
//
//		if (!nextId.empty()) {
//			ComboLink link;
//			link.requiredCommand = ActionCommand::Attack;
//			link.priority = 0;
//			link.nextAttackId = nextId;
//			def.comboLinks.push_back(link);
//		}
//		// nextIdが空(最終段)の場合はcomboLinksを空のままにする
//		// →ここでコンボが終了する。
//
//		return def;
//	}
//
//	PlayerAttackTable BuildDebugAttackTable()
//	{
//		PlayerAttackTable table;
//		table.attacks.push_back(MakeComboHit("APose_Attack02_1", "APose_Attack02_2", 0.18f, 0.35f, 1.05f, 10.0f));
//		table.attacks.push_back(MakeComboHit("APose_Attack02_2", "APose_Attack02_3", 0.18f, 0.35f, 1.05f, 10.0f));
//		table.attacks.push_back(MakeComboHit("APose_Attack02_3", "APose_Attack02_4", 0.2f, 0.15f, 0.3f, 12.0f));
//		table.attacks.push_back(MakeComboHit("APose_Attack02_4", "APose_Attack02_5", 0.25f, 0.15f, 0.35f, 12.0f));
//		table.attacks.push_back(MakeComboHit("APose_Attack02_5", "APose_Attack02_6", 0.25f, 0.2f, 0.35f, 14.0f));
//		table.attacks.push_back(MakeComboHit("APose_Attack02_6", "", 0.3f, 0.2f, 0.5f, 20.0f)); // 最終段(フィニッシュ)
//		return table;
//	}
//
//	// 旧CreateDebugEvadeData()。EvadeData構造体自体のデフォルト値と
//	// useRootMotionだけが異なる(こちらはtrue)ため、struct既定値に頼らず
//	// ここで明示的に組み立てる。
//	EvadeData BuildDebugEvadeData()
//	{
//		EvadeData data{};
//		data.activeDuration = 0.25f;
//		data.recoveryDuration = 0.15f;
//		data.justWindowStart = 0.05f;
//		data.justWindowEnd = 0.15f;
//		data.evadeDistance = 3.0f;
//		data.useRootMotion = true;
//
//		data.animationNameForward = "APose_Slide_F";
//		data.animationNameBackward = "APose_Slide_B";
//		data.animationNameLeft = "APose_Slide_L";
//		data.animationNameRight = "APose_Slide_R";
//
//		return data;
//	}
//
//	// 旧CreateDebugGuardData()。justWindowDuration/startDurationが
//	// GuardData構造体のデフォルト値と異なるため、同じくここで明示的に
//	// 組み立てる。
//	GuardData BuildDebugGuardData()
//	{
//		GuardData data{};
//		data.justWindowDuration = 0.15f;
//
//		data.animationName = "APose2DefenseL";
//		data.startDuration = 0.1f;
//		data.loopAnimationName = "DefenseL_Loop";
//
//		data.parrySuccessAnimationName = "DefenseL_Parry01";
//		data.parrySuccessDuration = 0.8f;
//
//		data.guardHitAnimationName = "DefenseL_Hit01";
//		data.guardHitDuration = 0.9f;
//
//		return data;
//	}
//}
//
//PlayerCombatBehaviorDefinition CreateDebugPlayerCombatBehavior()
//{
//	PlayerCombatBehaviorDefinition behavior;
//	behavior.attackTable = BuildDebugAttackTable();
//	behavior.evade = BuildDebugEvadeData();
//	behavior.guard = BuildDebugGuardData();
//	return behavior;
//}
//
//PlayerMovementAnimationDefinition CreateDebugPlayerMovementAnimations()
//{
//	// 旧PlayerMovementAnimationComponent.h内のメンバ初期化子から移設。
//	// コンポーネント自身はデータを「持たされる」側に徹し、
//	// SetMovementAnimations()経由で注入されるようになった。
//	PlayerMovementAnimationDefinition def;
//
//	def.walk = WalkAnimationSet{
//		MovementPhaseClips{
//			"APose_Strafe_Walk_F_Start", 0.15f,
//			"APose_Strafe_Walk_F_Loop",
//			"APose_Strafe_Walk_F_End", 0.15f
//		},
//		WalkLockedAnimationSet{
//			"APose_Strafe_Walk_F_Loop",
//			"APose_Strafe_Walk_FR",
//			"APose_Strafe_Walk_R",
//			"APose_Strafe_Walk_BR",
//			"APose_Strafe_Walk_B",
//			"APose_Strafe_Walk_BL",
//			"APose_Strafe_Walk_L",
//			"APose_Strafe_Walk_FL",
//		}
//	};
//
//	def.run = MovementPhaseClips{
//		"APose_Strafe_Run_F_Start", 0.15f,
//		"APose_Strafe_Run_F_Loop",
//		"APose_Strafe_Run_F_End", 0.15f
//	};
//
//	def.turn = TurnAnimationSet{
//		MotionClipData{ 0.25f, "APose_TurnL90",  true, 0.1f },
//		MotionClipData{ 0.25f, "APose_TurnR90",  true, 0.1f },
//		MotionClipData{ 0.35f, "APose_TurnL180", true, 0.1f },
//		MotionClipData{ 0.35f, "APose_TurnR180", true, 0.1f },
//	};
//
//	return def;
//}
