#pragma once
#include <string>
#include <vector>
#include "Application/Effect/SlashTrail/SlashTrailDispatcher.h"	// ※実際のパスに合わせて修正

// 初期設定(Prefab/JSONから渡す値)。base/tipはローカル座標。emitOnStartがtrueならAwakeで発光を開始する。
struct SlashTrailConfig
{
	std::string   trailName;
	Math::Vector3 base = { 0.0f, 0.0f, 0.0f };
	Math::Vector3 tip = { 0.0f, 0.0f, 0.0f };
	bool          emitOnStart = false;
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 剣の軌跡の「記録」を担当するコンポーネント。1コンポーネントで複数のリボンを持てる。
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// リボンごとにSlashTrailRecorderを持ち、毎フレームTip/Baseをワールド座標で記録して、
// 経路をDispatcherの描画スロット(ハンドル)へ渡す。頂点生成・消去演出・描画はDispatcher側。
// 記録終了(StopEmit/最大記録時間)で経路を確定してDispatcherへ渡せば、Component破棄後もフェードアウトが続く。
//
// 【使い方】
//   AddRibbon(config);  StartEmit(i);  ...  StopEmit(i);
//   従来のSetConfig/SetBaseTip/StartEmit/StopEmit(引数なし)はリボン0が対象。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class SlashTrailComponent : public ComponentBase {
public:
	using Config = SlashTrailConfig;

	explicit SlashTrailComponent(GameObject* owner, const std::string& trailName = "")
		: ComponentBase(owner)
	{
		ribbons_.emplace_back();
		ribbons_[0].trailName = trailName;
	}

	// 破棄時は発光中のリボンを確定させ、描画側でフェードアウトさせる(Dispatcherが先に破棄されないこと)
	~SlashTrailComponent() override { StopEmitAll(); }

	// Prefab読み込み用：リボンを1本だけに作り直す
	void SetConfig(const Config& config)
	{
		StopEmitAll();
		ribbons_.clear();
		AddRibbon(config);
	}

	// リボンを追加して、そのインデックスを返す
	size_t AddRibbon(const Config& config)
	{
		Ribbon ribbon;
		ribbon.trailName = config.trailName;
		ribbon.base = config.base;
		ribbon.tip = config.tip;
		ribbon.emitOnStart = config.emitOnStart;
		ribbons_.push_back(std::move(ribbon));
		return ribbons_.size() - 1;
	}

	size_t GetRibbonCount() const { return ribbons_.size(); }

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		for (size_t i = 0; i < ribbons_.size(); ++i)
		{
			if (ribbons_[i].emitOnStart) StartEmit(i);
		}
	}

	// ゲーム時間のdtで記録する(ヒットストップ中は記録も止まる)
	void Update(float dt) override
	{
		if (transform_ == nullptr || dispatcher_ == nullptr) return;

		// ローカル座標のTip/Baseをワールド座標に変換して記録する
		const Math::Matrix& worldMat = transform_->GetWorldMatrix();

		for (Ribbon& ribbon : ribbons_)
		{
			if (!ribbon.emitting) continue;

			const Math::Vector3 worldTip = Math::Vector3::Transform(ribbon.tip, worldMat);
			const Math::Vector3 worldBase = Math::Vector3::Transform(ribbon.base, worldMat);

			ribbon.recorder.UpdateTipBase(worldTip, worldBase);
			ribbon.recorder.Tick(dt);

			// 最大記録時間で記録器が自動終了した場合も、ここで確定して描画側へ渡す
			if (!ribbon.recorder.IsRecording())
			{
				FinishRibbon(ribbon);
				continue;
			}

			dispatcher_->SubmitLive(ribbon.handle, ribbon.recorder);
		}
	}

	// ----- リボン0向け(従来API) -----
	void SetBaseTip(Math::Vector3 base, Math::Vector3 tip) { SetBaseTip(0, base, tip); }
	void StartEmit() { StartEmit(0); }
	void StopEmit() { StopEmit(0); }

	// ----- リボン指定 -----
	void SetBaseTip(size_t index, Math::Vector3 base, Math::Vector3 tip)
	{
		if (index >= ribbons_.size()) return;
		ribbons_[index].base = base;
		ribbons_[index].tip = tip;
	}

	void StartEmit(size_t index)
	{
		if (index >= ribbons_.size()) return;

		Ribbon& ribbon = ribbons_[index];
		if (ribbon.emitting) return;

		ResolveDispatcher();
		if (dispatcher_ == nullptr) return;

		// 定義は開始時に引く(Component生成後に登録された定義でも使える)
		const SlashTrailDefId defId = dispatcher_->FindDefinition(ribbon.trailName);
		const SlashTrailParams* params = dispatcher_->GetDefinition(defId);
		if (params == nullptr) return;

		ribbon.handle = dispatcher_->Acquire(defId);
		if (!ribbon.handle.IsValid()) return;

		ribbon.recorder.Init(*params);
		ribbon.recorder.Begin();
		ribbon.emitting = true;
	}

	void StopEmit(size_t index)
	{
		if (index >= ribbons_.size()) return;
		if (!ribbons_[index].emitting) return;

		FinishRibbon(ribbons_[index]);
	}

	void StartEmitAll() { for (size_t i = 0; i < ribbons_.size(); ++i) StartEmit(i); }
	void StopEmitAll() { for (size_t i = 0; i < ribbons_.size(); ++i) StopEmit(i); }

private:
	// リボン1本ぶんの設定・記録・描画スロット
	struct Ribbon
	{
		std::string        trailName;
		Math::Vector3      base{};
		Math::Vector3      tip{};
		bool               emitOnStart = false;

		bool               emitting = false;
		SlashTrailRecorder recorder;
		SlashTrailHandle   handle;
	};

	// 記録を確定して描画側へ渡し、このリボンを待機状態へ戻す
	void FinishRibbon(Ribbon& ribbon)
	{
		ribbon.recorder.End();
		if (dispatcher_) dispatcher_->Finish(ribbon.handle, ribbon.recorder);

		ribbon.handle = SlashTrailHandle{};
		ribbon.emitting = false;
	}

	// Dispatcherの取得先はここだけ(Contextへの持たせ方に合わせて書き換える)
	void ResolveDispatcher()
	{
		if (dispatcher_ != nullptr) return;
		dispatcher_ = GetOwner()->GetContext()->slashTrailDispatcher;
	}

	TransformComponent*   transform_ = nullptr;
	SlashTrailDispatcher* dispatcher_ = nullptr;

	std::vector<Ribbon>   ribbons_;
};
