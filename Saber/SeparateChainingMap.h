#pragma once

#include <algorithm>
#include <functional>
#include <unordered_map>

template <typename Key, typename Value>
class UnorderedSeparateChainingMap {
	struct VectorMutex {
		std::vector<Value> values{};
		std::mutex mutex{};
	};

	std::unordered_map<Key, std::shared_ptr<VectorMutex>> m_map{};
	std::mutex m_mapMutex{};

public:
	UnorderedSeparateChainingMap() = default;
	UnorderedSeparateChainingMap(size_t mapSize) {
		m_map.reserve(mapSize);
	}

	void Add(Key key, Value value) {
		if (!m_map.contains(key)) {
			std::scoped_lock<std::mutex> mapLock{ m_mapMutex };
			m_map.insert(std::make_pair(key, std::make_shared<VectorMutex>()));
		}
		std::scoped_lock<std::mutex> vectorLock{ m_map[key]->mutex };
		m_map[key]->values.push_back(value);
	}

	void Remove(Key key, size_t valueId) {
		assert(m_map.contains(key));
		std::scoped_lock<std::mutex> vectorLock{ m_map[key]->mutex };
		assert(valueId < m_map[key]->values.size());
		m_map[key]->values[valueId] = m_map[key]->values.back();
		m_map[key]->values.pop_back();
	}

	void ForEachValue(std::function<void(Value& value)> function) {
		std::scoped_lock<std::mutex> mapLock{ m_mapMutex };
		for (auto& [_, value] : m_map) {
			std::scoped_lock<std::mutex> vectorLock(value->mutex);
			std::for_each(value->values.begin(), value->values.end(), function);
		}
	}
};
