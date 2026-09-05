#pragma once

#include "TimeScaleEvents.h"

class TimeScaleSystem
{
public:
	explicit TimeScaleSystem(EventBus& bus, ObjectManager& objManager)
		: eventBus_(bus), objManager_(objManager)
	{
		using namespace Events::TimeScale;

		subscriptions_.push_back(ScopedSubscriber(&eventBus_,
			eventBus_.Subscribe<SetGlobalTimeScaleEvent>(
				[this](const SetGlobalTimeScaleEvent& e) { OnSetGlobalTimeScale(e); })));

		subscriptions_.push_back(ScopedSubscriber(&eventBus_,
			eventBus_.Subscribe<SetMaskTimeScaleEvent>(
				[this](const SetMaskTimeScaleEvent& e) { OnSetMaskTimeScale(e); })));

		subscriptions_.push_back(ScopedSubscriber(&eventBus_,
			eventBus_.Subscribe<SetObjectTimeScaleEvent>(
				[this](const SetObjectTimeScaleEvent& e) { OnSetObjectTimeScale(e); })));

		subscriptions_.push_back(ScopedSubscriber(&eventBus_,
			eventBus_.Subscribe<CancelTimeScaleRequestEvent>(
				[this](const CancelTimeScaleRequestEvent& e) { OnCancelRequest(e); })));
	}

	void Update(float dt)
	{
		std::vector<uint64_t> expired;

		Tick(globalRequests_, dt, expired);

		for (auto& [mask, list] : maskRequests_)
		{
			Tick(list, dt, expired);
		}

		for (auto& [handle, list] : objectRequests_)
		{
			if (!handle.IsValid())
			{
				for (const auto& r : list) expired.push_back(r.id);
				continue;
			}
			Tick(list, dt, expired);
		}

		for (uint64_t id : expired)
		{
			CancelRequest(id);
		}
	}

private:
	// --- 内部データ構造 ---------------------------------------------

	// timeScale操作1件分。1回のSet*TimeScaleEventにつき1つ生成される。
	struct TimeScaleRequest
	{
		uint64_t id = 0;
		float scale = 1.0f;
		float remaining = -1.0f; // 負の値 = 無期限
	};

	// CancelTimeScaleRequestEventが来た時、どのコンテナのどのキーに
	// 対応する要求かを逆引きするための情報。
	enum class RequestCategory : uint8_t { Global, Mask, Object };
	struct RequestLocation
	{
		RequestCategory category = RequestCategory::Global;
		uint8_t mask = 0;
		Handle<GameObject> object;
	};

	// --- イベントハンドラ ---------------------------------------------

	void OnSetGlobalTimeScale(const Events::TimeScale::SetGlobalTimeScaleEvent& e)
	{
		const uint64_t id = IssueRequestId();
		globalRequests_.push_back({ id, e.timeScale, e.duration });
		requestLocations_[id] = RequestLocation{ RequestCategory::Global, 0, {} };
		ApplyGlobal();
		NotifyIssued(id);
	}

	void OnSetMaskTimeScale(const Events::TimeScale::SetMaskTimeScaleEvent& e)
	{
		const uint64_t id = IssueRequestId();
		maskRequests_[e.mask].push_back({ id, e.timeScale, e.duration });
		requestLocations_[id] = RequestLocation{ RequestCategory::Mask, e.mask, {} };
		ApplyMask(e.mask);
		NotifyIssued(id);
	}

	void OnSetObjectTimeScale(const Events::TimeScale::SetObjectTimeScaleEvent& e)
	{
		if (!e.target.IsValid()) return;

		const uint64_t id = IssueRequestId();
		objectRequests_[e.target].push_back({ id, e.timeScale, e.duration });
		requestLocations_[id] = RequestLocation{ RequestCategory::Object, 0, e.target };
		ApplyObject(e.target);
		NotifyIssued(id);
	}

	void OnCancelRequest(const Events::TimeScale::CancelTimeScaleRequestEvent& e)
	{
		CancelRequest(e.requestId);
	}

	// --- 要求の発行・解除 -----------------------------------------------

	uint64_t IssueRequestId() { return nextRequestId_++; }

	void NotifyIssued(uint64_t id)
	{
		Events::TimeScale::TimeScaleRequestIssuedEvent issued;
		issued.requestId = id;
		eventBus_.Publish(issued);
	}

	// idで指定された要求を1件だけ取り除き、影響を受けるGlobal/Mask/Objectを
	// 再計算して適用し直す。
	void CancelRequest(uint64_t id)
	{
		auto locIt = requestLocations_.find(id);
		if (locIt == requestLocations_.end()) return;

		const RequestLocation loc = locIt->second;
		requestLocations_.erase(locIt);

		switch (loc.category)
		{
		case RequestCategory::Global:
			RemoveById(globalRequests_, id);
			ApplyGlobal();
			break;

		case RequestCategory::Mask:
		{
			auto it = maskRequests_.find(loc.mask);
			if (it != maskRequests_.end()) RemoveById(it->second, id);
			ApplyMask(loc.mask);
			break;
		}

		case RequestCategory::Object:
		{
			auto it = objectRequests_.find(loc.object);
			if (it != objectRequests_.end()) RemoveById(it->second, id);
			ApplyObject(loc.object);
			break;
		}
		}
	}

	static void RemoveById(std::vector<TimeScaleRequest>& list, uint64_t id)
	{
		list.erase(std::remove_if(list.begin(), list.end(),
			[id](const TimeScaleRequest& r) { return r.id == id; }),
			list.end());
	}

	// remaining>=0の要求だけカウントダウンし、切れたものをexpiredへ積む。
	static void Tick(std::vector<TimeScaleRequest>& list, float dt, std::vector<uint64_t>& expired)
	{
		for (auto& r : list)
		{
			if (r.remaining < 0.0f) continue; // 無期限
			r.remaining -= dt;
			if (r.remaining <= 0.0f) expired.push_back(r.id);
		}
	}

	// 生きている要求群の中から最も強い効果(最小のtimeScale)を選ぶ。
	// 要求が1つも無ければ通常速度(1.0)に戻す。
	static float ComputeAggregate(const std::vector<TimeScaleRequest>& list)
	{
		if (list.empty()) return 1.0f;
		float result = list.front().scale;
		for (const auto& r : list) result = std::min(result, r.scale);
		return result;
	}

	void ApplyGlobal()
	{
		objManager_.SetTimeScale(ComputeAggregate(globalRequests_));
	}

	void ApplyMask(uint8_t mask)
	{
		auto it = maskRequests_.find(mask);
		const float scale = (it != maskRequests_.end()) ? ComputeAggregate(it->second) : 1.0f;
		objManager_.SetTimeScale(scale, mask);
	}

	void ApplyObject(const Handle<GameObject>& handle)
	{
		GameObject* obj = handle.Resolve();
		if (!obj) return;

		auto it = objectRequests_.find(handle);
		const float scale = (it != objectRequests_.end()) ? ComputeAggregate(it->second) : 1.0f;
		obj->SetTimeScale(scale);
	}

	// --- メンバ ---------------------------------------------------------

	EventBus& eventBus_;
	ObjectManager& objManager_;
	std::vector<ScopedSubscriber> subscriptions_;

	std::vector<TimeScaleRequest> globalRequests_;
	std::unordered_map<uint8_t, std::vector<TimeScaleRequest>> maskRequests_;
	std::unordered_map<Handle<GameObject>, std::vector<TimeScaleRequest>> objectRequests_;

	std::unordered_map<uint64_t, RequestLocation> requestLocations_;

	uint64_t nextRequestId_ = 1;
};