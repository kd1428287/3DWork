#pragma once
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cstdint>

using SubscriptionId = uint64_t;
class Event;

class EventBus
{
private:
	std::atomic<bool> m_destroyed{ false };
	using HandlerFunction = std::function<void(const Event&)>;

	// IDと関数をペアにして保持する
	using IdHandlerPair = std::pair<SubscriptionId, HandlerFunction>;
	std::unordered_map<std::type_index, std::vector<IdHandlerPair>> subscribers;

	mutable std::mutex m_mutex;  // subscribers を保護するミューテックス

	// 一意のIDを発行するためのカウンタ
	std::atomic<SubscriptionId> nextId;

public:
	EventBus() : nextId(1) {}; // IDは1から開始

	// コピー・ムーブ禁止（mutex を持つため）
	EventBus(const EventBus&) = delete;
	EventBus& operator=(const EventBus&) = delete;

	~EventBus() {
		m_destroyed.store(true, std::memory_order_release); // 破棄前にフラグを立てる
		std::lock_guard<std::mutex> lock(m_mutex);
		subscribers.clear();
	}

	// 戻り値として SubscriptionId を返すようにする
	template <typename T>
	SubscriptionId Subscribe(std::function<void(const T&)> callback) {
		auto wrapper = [callback](const Event& e) {
			callback(static_cast<const T&>(e));
			};

		// IDの発行自体はロック不要（atomic）
		SubscriptionId id = nextId.fetch_add(1, std::memory_order_relaxed);

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			subscribers[std::type_index(typeid(T))].push_back({ id, std::move(wrapper) });
		}

		return id; // 解除に必要なIDを呼び出し元に返す
	}

	bool IsDestroyed() const { return m_destroyed.load(std::memory_order_acquire); }

	void Unsubscribe(SubscriptionId id) {
		if (id == 0 || IsDestroyed()) return;

		std::lock_guard<std::mutex> lock(m_mutex);
		// すべてのイベント型のリストから、指定されたIDを持つものを探して削除
		for (auto& [type, handlerList] : subscribers) {
			auto it = std::find_if(handlerList.begin(), handlerList.end(),
				[id](const IdHandlerPair& p) { return p.first == id; });
			if (it != handlerList.end()) {
				handlerList.erase(it);
				return; // IDは一意なので見つかったら終了
			}
		}
	}

	template <typename T>
	void Publish(const T& event) {
		if (IsDestroyed()) return;

		// 1. ロックを取って対象のハンドラ一覧だけコピーする
		std::vector<IdHandlerPair> handlerList;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = subscribers.find(std::type_index(typeid(T)));
			if (it == subscribers.end()) return;
			handlerList = it->second; // コピーしてからイテレート
		}

		// 2. ロックを解放した状態でハンドラを実行する
		//    （コールバック内で subscribe/unsubscribe/publish が呼ばれても
		//      デッドロックしないようにするため、実行中はロックを保持しない）
		for (auto& pair : handlerList) {
			if (IsDestroyed()) return;

			bool stillValid = false;
			{
				// 実行直前に、購読がまだ有効か再確認する
				std::lock_guard<std::mutex> lock(m_mutex);
				auto it = subscribers.find(std::type_index(typeid(T)));
				if (it != subscribers.end()) {
					stillValid = std::any_of(it->second.begin(), it->second.end(),
						[&](const IdHandlerPair& p) { return p.first == pair.first; });
				}
			}

			if (stillValid) {
				pair.second(event); // ロックなしで実行
			}
		}
	}

	void Release() {
		std::lock_guard<std::mutex> lock(m_mutex);
		subscribers.clear();
	}
};

class GlobalEventBus
{
public:
	static EventBus& Instance()
	{
		static EventBus instance;
		return instance;
	}
private:
	GlobalEventBus() = default;
	~GlobalEventBus() = default;
};

#define GLOBALEVENT GlobalEventBus::Instance()

class ScopedSubscriber {
private:
	EventBus* m_bus = nullptr; // 登録先のバスを指すポインタ
	SubscriptionId m_id = 0;

public:
	ScopedSubscriber() = default;

	// 引数で「どのバスの」「どのIDか」を受け取る
	ScopedSubscriber(EventBus* bus, SubscriptionId id) : m_bus(bus), m_id(id) {}

	// コピー禁止
	ScopedSubscriber(const ScopedSubscriber&) = delete;
	ScopedSubscriber& operator=(const ScopedSubscriber&) = delete;

	// ムーブ許可
	ScopedSubscriber(ScopedSubscriber&& other) noexcept
		: m_bus(other.m_bus), m_id(other.m_id)
	{
		other.m_bus = nullptr;
		other.m_id = 0;
	}

	ScopedSubscriber& operator=(ScopedSubscriber&& other) noexcept {
		if (this != &other) {
			reset();
			m_bus = other.m_bus;
			m_id = other.m_id;
			other.m_bus = nullptr;
			other.m_id = 0;
		}
		return *this;
	}

	~ScopedSubscriber() { reset(); }

	void reset() {
		// バスが生きていて、IDが有効なら解除
		if (m_bus && m_id != 0 && !m_bus->IsDestroyed()) {
			m_bus->Unsubscribe(m_id);
		}
		m_bus = nullptr;
		m_id = 0;
	}
};