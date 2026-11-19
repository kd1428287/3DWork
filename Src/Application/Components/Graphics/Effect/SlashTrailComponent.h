#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "Application/Effect/SlashTrail/SlashTrailDispatcher.h"	// ※実際のパスに合わせて修正

// 軌跡の根元(base)と先端(tip)。部位のローカル座標。
struct SlashTrailShape
{
	Math::Vector3 base = { 0.0f, 0.0f, 0.0f };
	Math::Vector3 tip = { 0.0f, 0.0f, 0.0f };
};

// 初期設定(Prefab/JSONから渡す値)。形状は"Default"を必ず含めること(キー未指定/emitOnStartで使う)。
struct SlashTrailConfig
{
	std::string   trailName;
	bool          emitOnStart = false;
	std::unordered_map<std::string, SlashTrailShape> shapes; // 攻撃から名前で選ぶ形状
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 剣の軌跡の「記録」を担当するコンポーネント。1コンポーネントで複数のリボンを持てる。
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// リボンごとにSlashTrailRecorderを持ち、毎フレームTip/Baseをワールド座標で記録して、
// 経路をDispatcherの描画スロット(ハンドル)へ渡す。頂点生成・消去演出・描画はDispatcher側。
// 記録終了(StopEmit/最大記録時間)で経路を確定してDispatcherへ渡せば、Component破棄後もフェードアウトが続く。
//
// 【使い方】
//   攻撃ごと : StartEmitAll(trailName, shapeKey);  ...  StopEmitAll();
//   リボン指定: AddRibbon(config);  StartEmit(i);  ...  StopEmit(i);
//   引数なしのStartEmit/StopEmitはリボン0が対象。
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

	// Prefab読み込み用：リボンとシェイプ表を作り直す(リボンは1本になる)
	void SetConfig(const Config& config)
	{
		StopEmitAll();
		ribbons_.clear();
		shapes_.clear();
		AddRibbon(config);
		assert(shapes_.count(kDefaultShape) && "SlashTrailComponent: Defaultシェイプが未登録");
	}

	// シェイプを登録(同じキーは上書き)
	void RegisterShape(const std::string& key, const SlashTrailShape& shape) { shapes_[key] = shape; }

	// リボンを追加して、そのインデックスを返す。config.shapesは既存キーを上書きせず追加する
	size_t AddRibbon(const Config& config)
	{
		Ribbon ribbon;
		ribbon.trailName = config.trailName;
		ribbon.emitOnStart = config.emitOnStart;
		ribbons_.push_back(std::move(ribbon));

		shapes_.insert(config.shapes.begin(), config.shapes.end());
		return ribbons_.size() - 1;
	}

	size_t GetRibbonCount() const { return ribbons_.size(); }

	void Awake() override
	{
		transform_ = GetOwner()->GetComponent<TransformComponent>();

		// 常時発光のリボンはDefaultシェイプで開始する
		const SlashTrailShape* defaultShape = FindShape("");
		for (size_t i = 0; i < ribbons_.size(); ++i)
		{
			if (!ribbons_[i].emitOnStart) continue;
			ApplyShape(ribbons_[i], defaultShape);
			StartEmit(i);
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

			// 先に時間を進める(今フレームの移動にかかった時間を、速度(太さ)の計算に含める為)
			ribbon.recorder.Tick(dt);

			// 最大記録時間で記録器が自動終了した場合も、ここで確定して描画側へ渡す
			if (!ribbon.recorder.IsRecording())
			{
				FinishRibbon(ribbon);
				continue;
			}

			ribbon.recorder.UpdateTipBase(worldTip, worldBase);

			dispatcher_->SubmitLive(ribbon.handle, ribbon.recorder);
		}
	}

	// ----- リボン0向け -----
	void StartEmit() { StartEmit(0); }
	void StopEmit() { StopEmit(0); }

	// ----- リボン指定 -----
	// base/tipの直接指定(デバッグ調整用)。通常はシェイプのキーで指定する
	void SetBaseTip(size_t index, Math::Vector3 base, Math::Vector3 tip)
	{
		if (index >= ribbons_.size()) return;
		ribbons_[index].base = base;
		ribbons_[index].tip = tip;
	}

	// 現在のtrailName/base/tipで発光を開始する
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

	void StopEmitAll() { for (size_t i = 0; i < ribbons_.size(); ++i) StopEmit(i); }

	// ----- 攻撃ごと(キー指定) -----
	// trailNameが空なら軌跡なし。shapeKeyが空/未登録ならDefaultを使う。発光中のリボンは触らない。
	void StartEmitAll(const std::string& trailName, const std::string& shapeKey)
	{
		if (trailName.empty()) return;

		const SlashTrailShape* shape = FindShape(shapeKey);
		for (size_t i = 0; i < ribbons_.size(); ++i)
		{
			Ribbon& ribbon = ribbons_[i];
			if (ribbon.emitting) continue;

			ribbon.trailName = trailName;
			ApplyShape(ribbon, shape);
			StartEmit(i);
		}
	}

private:
	static constexpr const char* kDefaultShape = "Default";

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

	// 空キーはDefault。非空で未登録のキーはタイポとみなしてassertし、Releaseでは黙ってDefaultへ落とす
	const SlashTrailShape* FindShape(const std::string& key) const
	{
		auto it = shapes_.find(key.empty() ? kDefaultShape : key);
		if (it == shapes_.end())
		{
			assert(key.empty() && "SlashTrailComponent: 未登録のtrailShapeキー");
			it = shapes_.find(kDefaultShape);
		}
		return (it != shapes_.end()) ? &it->second : nullptr;
	}

	// シェイプのbase/tipをリボンへ反映する(nullptrなら何もしない)
	static void ApplyShape(Ribbon& ribbon, const SlashTrailShape* shape)
	{
		if (shape == nullptr) return;
		ribbon.base = shape->base;
		ribbon.tip = shape->tip;
	}

	// Dispatcherの取得先はここだけ(Contextへの持たせ方に合わせて書き換える)
	void ResolveDispatcher()
	{
		if (dispatcher_ != nullptr) return;
		dispatcher_ = GetOwner()->GetContext()->slashTrailDispatcher;
	}

	TransformComponent* transform_ = nullptr;
	SlashTrailDispatcher* dispatcher_ = nullptr;

	std::vector<Ribbon>   ribbons_;
	std::unordered_map<std::string, SlashTrailShape> shapes_;
};