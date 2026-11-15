#include "IndexRange.h"

#include <bit>
#include <iterator>

// StackIndexRange

size_t StackIndexRange::Allocate() {
	assert(m_size < m_capacity);
	return m_size++;
}

void StackIndexRange::Free(size_t id) {
	assert(id == m_size - 1);
	--m_size;
}

void StackIndexRange::FreeAll() {
	m_size = 0;
}

// PoolIndexRange

PoolIndexRange::PoolIndexRange(size_t capacity) : IndexRange(capacity) {
	FreeAll();
}

size_t PoolIndexRange::Allocate() {
	assert(!m_freeIds.empty());

	const size_t id{ m_freeIds.back() };
	m_freeIds.pop_back();

	return id;
}

void PoolIndexRange::Free(size_t id) {
	assert(id < m_capacity);
	m_freeIds.push_back(id);
}

void PoolIndexRange::FreeAll() {
	m_freeIds.clear();
	m_freeIds.reserve(m_capacity);
	for (size_t i{}; i < m_capacity; ++i) {
		m_freeIds.push_back(m_capacity - i - 1);
	}
}

// RangeAllocator

RangeAllocator::RangeAllocator(size_t capacity) {
	Grow(capacity);
}

std::optional<size_t> RangeAllocator::Allocate(size_t size) {
	assert(size);

	for (auto it{ m_freeRanges.begin() }; it != m_freeRanges.end(); ++it) {
		const auto [first, freeSize] { *it };
		if (freeSize < size) {
			continue;
		}

		m_freeRanges.erase(it);
		if (freeSize > size) {
			m_freeRanges.emplace(first + size, freeSize - size);
		}
		return first;
	}

	return std::nullopt;
}

void RangeAllocator::Free(size_t first, size_t size) {
	assert(size);

	auto next{ m_freeRanges.lower_bound(first) };
	assert(next == m_freeRanges.end() || first + size <= next->first);

	if (next != m_freeRanges.end() && first + size == next->first) {
		size += next->second;
		next = m_freeRanges.erase(next);
	}

	if (next != m_freeRanges.begin()) {
		auto prev{ std::prev(next) };
		assert(prev->first + prev->second <= first);

		if (prev->first + prev->second == first) {
			prev->second += size;
			return;
		}
	}

	m_freeRanges.emplace(first, size);
}

void RangeAllocator::Grow(size_t capacity) {
	assert(capacity >= m_capacity);
	if (capacity == m_capacity) {
		return;
	}

	const size_t oldCapacity{ m_capacity };
	m_capacity = capacity;
	Free(oldCapacity, capacity - oldCapacity);
}

// IdRange

IdRange::IdRange(std::shared_ptr<GrowingRangeAllocator> pAllocator, size_t first, size_t size)
	: m_pAllocator(std::move(pAllocator)),
	m_first(first),
	m_size(size)
{
	assert(m_pAllocator);
}

IdRange::~IdRange() {
	m_pAllocator->Free(m_first, m_size);
}

// GrowingRangeAllocator

GrowingRangeAllocator::GrowingRangeAllocator(size_t capacity)
	: m_allocator(capacity)
{}

std::unique_ptr<IdRange> GrowingRangeAllocator::Allocate(size_t size) {
	std::scoped_lock<std::mutex> lock(m_mutex);

	std::optional<size_t> first{ m_allocator.Allocate(size) };
	if (!first) {
		// A power of two, so ranges taken one by one cost amortized constant time
		m_allocator.Grow(std::bit_ceil(m_allocator.GetCapacity() + size));
		first = m_allocator.Allocate(size);
		assert(first);
	}

	return std::make_unique<IdRange>(shared_from_this(), *first, size);
}

void GrowingRangeAllocator::Free(size_t first, size_t size) {
	std::scoped_lock<std::mutex> lock(m_mutex);
	m_allocator.Free(first, size);
}
