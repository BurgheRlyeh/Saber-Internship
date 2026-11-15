#pragma once

#include "Headers.h"

#include <concepts>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

// Ids 0..capacity-1, what they address is up to the owner
class IndexRange {
protected:
	size_t m_capacity{};

public:
	explicit IndexRange(size_t capacity) : m_capacity(capacity) {}
	virtual ~IndexRange() = default;

	inline size_t GetCapacity() const { return m_capacity; }
	virtual size_t GetSize() const = 0;

	virtual size_t Allocate() = 0;
	virtual void Free(size_t id) = 0;
	virtual void FreeAll() = 0;
};

// Freed in reverse order of allocation
class StackIndexRange : public IndexRange {
	size_t m_size{};

public:
	using IndexRange::IndexRange;

	size_t GetSize() const override { return m_size; }

	size_t Allocate() override;
	void Free(size_t id) override;
	void FreeAll() override;
};

// Freed in any order
class PoolIndexRange : public IndexRange {
	std::vector<size_t> m_freeIds{};

public:
	explicit PoolIndexRange(size_t capacity);

	size_t GetSize() const override { return m_capacity - m_freeIds.size(); }

	size_t Allocate() override;
	void Free(size_t id) override;
	void FreeAll() override;
};

// An index range made from its capacity
template <typename T>
concept IndexRangeConcept = std::derived_from<T, IndexRange> && std::constructible_from<T, size_t>;

// Contiguous ranges of ids: first fit, neighbors merged on free
class RangeAllocator {
	std::map<size_t, size_t> m_freeRanges{};	// first id -> size
	size_t m_capacity{};

public:
	explicit RangeAllocator(size_t capacity);

	inline size_t GetCapacity() const { return m_capacity; }

	// First id, or nothing when no free range is large enough
	std::optional<size_t> Allocate(size_t size);
	void Free(size_t first, size_t size);

	// The ids up to the new capacity become free
	void Grow(size_t capacity);
};

class GrowingRangeAllocator;

// Contiguous ids taken from an allocator, given back on destruction
class IdRange {
	std::shared_ptr<GrowingRangeAllocator> m_pAllocator{};
	size_t m_first{};
	size_t m_size{};

public:
	IdRange(std::shared_ptr<GrowingRangeAllocator> pAllocator, size_t first, size_t size);
	~IdRange();

	IdRange(const IdRange&) = delete;
	IdRange& operator=(const IdRange&) = delete;

	IdRange(IdRange&&) = delete;
	IdRange& operator=(IdRange&&) = delete;

	inline size_t GetFirst() const { return m_first; }
	inline size_t GetSize() const { return m_size; }
};

// Hands out ranges that give themselves back, grows instead of failing. Thread-safe
class GrowingRangeAllocator : public std::enable_shared_from_this<GrowingRangeAllocator> {
	friend class IdRange;

	RangeAllocator m_allocator;
	std::mutex m_mutex{};

public:
	explicit GrowingRangeAllocator(size_t capacity);

	std::unique_ptr<IdRange> Allocate(size_t size);

private:
	void Free(size_t first, size_t size);
};
