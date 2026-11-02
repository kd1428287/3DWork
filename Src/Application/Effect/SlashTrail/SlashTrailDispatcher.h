#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "SlashTrailInstance.h"
#include "../Common/KdAssetsTextureProvider.h"

using SlashTrailDefId = uint32_t;
constexpr SlashTrailDefId kInvalidSlashTrailDef = UINT32_MAX;

// Acquire()で得る描画スロットへの参照。解放後のスロットを指す古いハンドルは無視される(世代番号で判定)
struct SlashTrailHandle
{
	uint32_t index = UINT32_MAX;
	uint32_t generation = 0;

	bool IsValid() const { return index != UINT32_MAX; }
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// トレイル(斬撃の軌跡)の描画スロット管理とフェードアウト
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 記録はComponent側のSlashTrailRecorderが行い、ここは「定義の登録」「描画スロットの貸し出し」
// 「経路を受け取って頂点生成・描画」「消え切ったスロットの回収」だけを担当する(イベントは使わない)。
// 定義が同じスロットは頂点バッファごと使い回す。Update()/Draw()は同一スレッドから呼ぶこと。
//
// 【使い方】
//   dispatcher.Init();
//   SlashTrailDefId id = dispatcher.RegisterDefinition("PlayerSwordTrail", params);
//   SlashTrailHandle h = dispatcher.Acquire(id);       // 記録開始時
//   dispatcher.SubmitLive(h, recorder);                // 記録中、毎フレーム
//   dispatcher.Finish(h, recorder);                    // 記録終了時(以後は自動で消えて回収される)
//   dispatcher.Update(deltaTime);                      // 毎フレーム(カメラ更新後)
//   dispatcher.Draw(ParticleDrawPass::Default);        // DrawLit内から
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class SlashTrailDispatcher
{
public:

	SlashTrailDispatcher() {}
	~SlashTrailDispatcher() { Release(); }

	// コピー禁止(SlashTrailInstanceがコピー禁止の為)
	SlashTrailDispatcher(const SlashTrailDispatcher&) = delete;
	SlashTrailDispatcher& operator=(const SlashTrailDispatcher&) = delete;

	bool Init();

	// 定義・スロットを全て破棄する(発行済みのハンドルは全て無効になる)
	void Release();

	// ----- 定義 -----

	// 登録して定義IDを返す。同名なら上書きし、同じIDを返す
	SlashTrailDefId RegisterDefinition(const std::string& name, const SlashTrailParams& params);

	// 未登録ならkInvalidSlashTrailDef
	SlashTrailDefId FindDefinition(const std::string& name) const;

	// 無効なIDならnullptr
	const SlashTrailParams* GetDefinition(SlashTrailDefId id) const;

	// ----- 記録側(Component)から呼ぶ -----

	// 描画スロットを借りて記録中にする。失敗したら無効なハンドルを返す
	SlashTrailHandle Acquire(SlashTrailDefId id);

	// 記録中の経路を渡す(無効・解放済み・記録中でないハンドルは無視)
	void SubmitLive(SlashTrailHandle handle, const SlashTrailRecorder& recorder);

	// 確定した経路を渡して、消去(フェードアウト)へ移す。以後このハンドルは使わない
	void Finish(SlashTrailHandle handle, const SlashTrailRecorder& recorder);

	// ----- 毎フレーム -----

	// 全スロットの頂点更新→消え切ったスロットの回収
	void Update(float deltaTime);

	// DrawPassFlagsにpassが含まれるトレイルだけを描画する
	void Draw(ParticleDrawPass pass);

private:

	struct Definition
	{
		std::string			name;
		SlashTrailParams	params;
		uint32_t			version = 0;	// 上書きされるたびに増える(使い回し判定用)
	};

	struct Slot
	{
		SlashTrailInstance	instance;
		SlashTrailDefId		defId = kInvalidSlashTrailDef;
		uint32_t			defVersion = 0;
		uint32_t			generation = 0;
		bool				inUse = false;
	};

	// ハンドルが指す使用中スロットを返す(無効・世代不一致ならnullptr)
	Slot* Resolve(SlashTrailHandle handle);

	void ReleaseSlot(Slot& slot);

	std::vector<Definition>							definitions_;
	std::unordered_map<std::string, SlashTrailDefId>	nameToDef_;

	std::vector<Slot>	slots_;

	// パラメータのTexturePathからテクスチャを解決する(EffectDispatcherと同じ)
	KdAssetsTextureProvider textureProvider_;
};
