#pragma once

#include "Headers.h"

#include <vector>

template <typename T>
class Buffer;

template <typename T>
class BufferStorage {
protected:
	Buffer<T>& m_buffer;

public:
	BufferStorage(Buffer<T>& buffer) : m_buffer(buffer) {}
	virtual ~BufferStorage() = default;

	virtual T* GetData() = 0;
	virtual size_t GetDataSize() const = 0;

	virtual void UpdateAll(const T* pData, size_t count) = 0;
	virtual void UpdateAt(size_t id, const T& data) = 0;
};

template <typename T, typename Derived>
concept BufferStorageConcept = std::derived_from<Derived, BufferStorage<T>>;

template <typename T>
class WholeBufferStorage : public BufferStorage<T> {
	T m_data;
public:
	WholeBufferStorage(Buffer<T>& buffer) : BufferStorage<T>(buffer) {}

	virtual T* GetData() override {
		return &m_data;
	}
	virtual size_t GetDataSize() const override {
		return 1;
	}

	virtual void UpdateAll(const T* pData, size_t count) override {
		assert(count == 1);
		m_data = *pData;
	}
	virtual void UpdateAt(size_t id, const T& data) override {
		assert(id == 0);
		m_data = data;
	}
};

template <typename T>
class VectorBufferStorage : public BufferStorage<T> {
	std::vector<T> m_data{};

public:
	VectorBufferStorage(Buffer<T>& buffer) : BufferStorage<T>(buffer) {}

	virtual T* GetData() override {
		return m_data.data();
	}
	virtual size_t GetDataSize() const override {
		return m_data.size();
	}

	virtual void UpdateAll(const T* pData, size_t count) override {
		if (count > m_data.size()) {
			m_data.resize(count);
		}
		for (size_t i{}; i < count; ++i) {
			m_data[i] = pData[i];
		}
	}
	virtual void UpdateAt(size_t id, const T& data) override {
		if (id >= m_data.size()) {
			m_data.resize(id + 1);
		}
		m_data[id] = data;
	}
};
