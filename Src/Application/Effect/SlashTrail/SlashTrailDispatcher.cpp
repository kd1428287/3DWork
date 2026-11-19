#include "Application/main.h"
#include "SlashTrailDispatcher.h"
#include "Application/Definitions/Loaders/SlashTrailDataLoader.h"

#include <algorithm>

std::vector<SlashTrailDispatcher*> SlashTrailDispatcher::s_instances;
std::mutex SlashTrailDispatcher::s_instancesMutex;

// 初期化：JSONから定義を読み込み、再ロード通知の宛先に登録する
bool SlashTrailDispatcher::Init(const std::string& dataPath)
{
	dataPath_ = dataPath;

	LoadData();

	std::lock_guard<std::mutex> lock(s_instancesMutex);
	if (std::find(s_instances.begin(), s_instances.end(), this) == s_instances.end())
	{
		s_instances.push_back(this);
	}

	return true;
}

// 解放：定義とスロットを全て破棄する
void SlashTrailDispatcher::Release()
{
	{
		std::lock_guard<std::mutex> lock(s_instancesMutex);
		s_instances.erase(std::remove(s_instances.begin(), s_instances.end(), this), s_instances.end());
	}

	slots_.clear();
	definitions_.clear();
	nameToDef_.clear();
}

// JSONの各トレイルを定義として登録する(既存は同名で上書きされ、IDとスロットは維持される)
bool SlashTrailDispatcher::LoadData()
{
	SlashTrailDataFile data;
	if (!SlashTrailDataLoader::Load(dataPath_, data)) { return false; }

	for (const auto& def : data.Trails) { RegisterDefinition(def.Name, def.Params); }

	return true;
}

// 同じJSONを読んでいる全Dispatcherへ再ロード要求を立てる
void SlashTrailDispatcher::NotifyDataSaved(const std::string& path)
{
	std::lock_guard<std::mutex> lock(s_instancesMutex);

	for (SlashTrailDispatcher* dispatcher : s_instances)
	{
		if (dispatcher->dataPath_ == path)
		{
			dispatcher->reloadRequested_.store(true, std::memory_order_release);
		}
	}
}

// トレイル定義の登録：同名なら上書きして、同じIDを返す
SlashTrailDefId SlashTrailDispatcher::RegisterDefinition(const std::string& name, const SlashTrailParams& params)
{
	auto it = nameToDef_.find(name);
	if (it != nameToDef_.end())
	{
		Definition& def = definitions_[it->second];
		def.params = params;
		++def.version;
		return it->second;
	}

	const SlashTrailDefId id = static_cast<SlashTrailDefId>(definitions_.size());

	Definition def;
	def.name = name;
	def.params = params;
	definitions_.push_back(std::move(def));
	nameToDef_[name] = id;

	return id;
}

SlashTrailDefId SlashTrailDispatcher::FindDefinition(const std::string& name) const
{
	auto it = nameToDef_.find(name);
	return (it != nameToDef_.end()) ? it->second : kInvalidSlashTrailDef;
}

const SlashTrailParams* SlashTrailDispatcher::GetDefinition(SlashTrailDefId id) const
{
	return (id < definitions_.size()) ? &definitions_[id].params : nullptr;
}

// スロットを借りる：定義とバージョンが同じ空きスロットを優先して使い回す
SlashTrailHandle SlashTrailDispatcher::Acquire(SlashTrailDefId id)
{
	if (id >= definitions_.size()) { return {}; }

	const Definition& def = definitions_[id];

	size_t reusable = slots_.size();
	size_t anyFree = slots_.size();

	for (size_t i = 0; i < slots_.size(); ++i)
	{
		const Slot& slot = slots_[i];
		if (slot.inUse) { continue; }

		if (slot.defId == id && slot.defVersion == def.version)
		{
			reusable = i;
			break;
		}

		if (anyFree == slots_.size()) { anyFree = i; }
	}

	size_t index = reusable;
	bool needInit = false;

	if (index == slots_.size())
	{
		needInit = true;

		if (anyFree != slots_.size())
		{
			index = anyFree;
		}
		else
		{
			slots_.emplace_back();
			index = slots_.size() - 1;
		}
	}

	Slot& slot = slots_[index];

	if (needInit)
	{
		// Init失敗時に別定義の使い回しと誤認しないよう、先に無効化しておく
		slot.defId = kInvalidSlashTrailDef;

		if (!slot.instance.Init(def.params, &textureProvider_)) { return {}; }

		slot.defId = id;
		slot.defVersion = def.version;
	}

	slot.inUse = true;
	slot.instance.BeginLive();

	SlashTrailHandle handle;
	handle.index = static_cast<uint32_t>(index);
	handle.generation = slot.generation;
	return handle;
}

// 記録中の経路を渡す
void SlashTrailDispatcher::SubmitLive(SlashTrailHandle handle, const SlashTrailRecorder& recorder)
{
	Slot* slot = Resolve(handle);
	if (!slot) { return; }

	slot->instance.SetLivePath(recorder);
}

// 確定した経路を渡して消去へ移す
void SlashTrailDispatcher::Finish(SlashTrailHandle handle, const SlashTrailRecorder& recorder)
{
	Slot* slot = Resolve(handle);
	if (!slot) { return; }

	slot->instance.Finish(recorder);
}

// 毎フレーム更新：再ロード→頂点更新→消え切ったスロットの回収
void SlashTrailDispatcher::Update(float deltaTime)
{
	if (reloadRequested_.exchange(false, std::memory_order_acq_rel)) { LoadData(); }

	for (Slot& slot : slots_)
	{
		if (!slot.inUse) { continue; }

		slot.instance.Update(deltaTime);

		if (slot.instance.IsFinished()) { ReleaseSlot(slot); }
	}
}

// 描画：使用中のスロットのうち、passが含まれるものだけを描画する
void SlashTrailDispatcher::Draw(ParticleDrawPass pass)
{
	for (Slot& slot : slots_)
	{
		if (!slot.inUse) { continue; }

		slot.instance.Draw(pass);
	}
}

SlashTrailDispatcher::Slot* SlashTrailDispatcher::Resolve(SlashTrailHandle handle)
{
	if (!handle.IsValid() || handle.index >= slots_.size()) { return nullptr; }

	Slot& slot = slots_[handle.index];
	if (!slot.inUse || slot.generation != handle.generation) { return nullptr; }

	return &slot;
}

// 世代番号を進めて、古いハンドルを無効にする
void SlashTrailDispatcher::ReleaseSlot(Slot& slot)
{
	slot.inUse = false;
	++slot.generation;
}
