#pragma once
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cstdint>
#include <memory>

using SubscriptionId = uint64_t;
class Event;

// forward declaration
class ScopedSubscriber;

// std::enable_shared_from_this を継承して weak_ptr を取得可能にする
class EventBus : public std::enable_shared_from_this<EventBus>
{
private:
	std::atomic<bool> m_destroyed{ false };
	using HandlerFunction = std::function<void(const Event&)>;

	using IdHandlerPair = std::pair<SubscriptionId, HandlerFunction>;
	std::unordered_map<std::type_index, std::vector<IdHandlerPair>> subscribers;

	mutable std::mutex m_mutex;
	std::atomic<SubscriptionId> nextId;

public:
	EventBus() : nextId(1) {}

	EventBus(const EventBus&) = delete;
	EventBus& operator=(const EventBus&) = delete;

	~EventBus() {
		m_destroyed.store(true, std::memory_order_release);
		std::lock_guard<std::mutex> lock(m_mutex);
		subscribers.clear();
	}

	// イベント購読 (SubscriptionId を返す従来型)
	template <typename T>
	SubscriptionId Subscribe(std::function<void(const T&)> callback) {
		auto wrapper = [callback](const Event& e) {
			callback(static_cast<const T&>(e));
			};

		SubscriptionId id = nextId.fetch_add(1, std::memory_order_relaxed);

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			subscribers[std::type_index(typeid(T))].push_back({ id, std::move(wrapper) });
		}

		return id;
	}

	// 【追加】RAII管理された ScopedSubscriber を直接受け取る便利なヘルパー関数
	template <typename T>
	[[nodiscard]] ScopedSubscriber SubscribeScoped(std::function<void(const T&)> callback);

	bool IsDestroyed() const { return m_destroyed.load(std::memory_order_acquire); }

	void Unsubscribe(SubscriptionId id) {
		if (id == 0 || IsDestroyed()) return;

		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& [type, handlerList] : subscribers) {
			auto it = std::find_if(handlerList.begin(), handlerList.end(),
				[id](const IdHandlerPair& p) { return p.first == id; });
			if (it != handlerList.end()) {
				handlerList.erase(it);
				return;
			}
		}
	}

	template <typename T>
	void Publish(const T& event) {
		if (IsDestroyed()) return;

		std::vector<IdHandlerPair> handlerList;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = subscribers.find(std::type_index(typeid(T)));
			if (it == subscribers.end()) return;
			handlerList = it->second;
		}

		for (auto& pair : handlerList) {
			if (IsDestroyed()) return;

			bool stillValid = false;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto it = subscribers.find(std::type_index(typeid(T)));
				if (it != subscribers.end()) {
					stillValid = std::any_of(it->second.begin(), it->second.end(),
						[&](const IdHandlerPair& p) { return p.first == pair.first; });
				}
			}

			if (stillValid) {
				pair.second(event);
			}
		}
	}

	void Release() {
		std::lock_guard<std::mutex> lock(m_mutex);
		subscribers.clear();
	}
};


// --- std::weak_ptr を使用した ScopedSubscriber ---
class ScopedSubscriber {
private:
	std::weak_ptr<EventBus> m_bus; // 生ポインタではなく weak_ptr で保持
	SubscriptionId m_id = 0;

public:
	ScopedSubscriber() = default;

	// shared_ptr または weak_ptr を受け取るコンストラクタ
	ScopedSubscriber(std::weak_ptr<EventBus> bus, SubscriptionId id)
		: m_bus(std::move(bus)), m_id(id) {}

	// コピー禁止
	ScopedSubscriber(const ScopedSubscriber&) = delete;
	ScopedSubscriber& operator=(const ScopedSubscriber&) = delete;

	// ムーブ許可
	ScopedSubscriber(ScopedSubscriber&& other) noexcept
		: m_bus(std::move(other.m_bus)), m_id(other.m_id)
	{
		other.m_id = 0;
	}

	ScopedSubscriber& operator=(ScopedSubscriber&& other) noexcept {
		if (this != &other) {
			reset();
			m_bus = std::move(other.m_bus);
			m_id = other.m_id;
			other.m_id = 0;
		}
		return *this;
	}

	~ScopedSubscriber() { reset(); }

	void reset() {
		if (m_id != 0) {
			// lock() を呼び出し、EventBus がまだ生存している場合のみ参照・解除を行う
			if (auto bus = m_bus.lock()) {
				if (!bus->IsDestroyed()) {
					bus->Unsubscribe(m_id);
				}
			}
			// すでに bus が破棄されている場合は何もしない (ダングリングアクセスが発生しない)
		}
		m_bus.reset();
		m_id = 0;
	}

	SubscriptionId GetId() const { return m_id; }
};

class GlobalEventBus
{
public:
	static std::shared_ptr<EventBus> Instance()
	{
		// shared_ptr で保持するシングルトン
		static auto instance = std::make_shared<EventBus>();
		return instance;
	}

private:
	GlobalEventBus() = default;
};

#define GLOBALEVENT GlobalEventBus::Instance()

template <typename T>
inline ScopedSubscriber EventBus::SubscribeScoped(std::function<void(const T&)> callback) {
	SubscriptionId id = Subscribe<T>(std::move(callback));
	return ScopedSubscriber(weak_from_this(), id);
}