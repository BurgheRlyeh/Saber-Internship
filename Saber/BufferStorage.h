/**
 * @file BufferStorage.h
 * @brief CPU-side shadow copies of GPU buffer data, used by the updater pipeline.
 *
 * A @ref BufferStorage mirrors the contents of a @ref Buffer on the CPU so
 * that partial or full updates can be recorded and then batched to the GPU.
 */
#pragma once

#include "Headers.h"

#include <vector>

template <typename T>
class Buffer;

/**
 * @brief Abstract base for CPU-side buffer shadows.
 *
 * Subclasses choose the concrete storage strategy (single element, vector, …).
 *
 * @tparam T Element type matching the parent @ref Buffer.
 */
template <typename T>
class BufferStorage {
protected:
    Buffer<T>& m_buffer; /**< @brief Reference to the GPU buffer this storage shadows. */

public:
    /**
     * @brief Constructs the storage bound to the given buffer.
     * @param buffer GPU buffer to shadow.
     */
    BufferStorage(Buffer<T>& buffer) : m_buffer(buffer) {}
    virtual ~BufferStorage() = default;

    /** @brief Returns a pointer to the beginning of the CPU-side data. */
    virtual T* GetData() = 0;

    /** @brief Returns the number of valid elements in the CPU-side buffer. */
    virtual size_t GetDataSize() const = 0;

    /**
     * @brief Replaces all CPU-side data with the supplied array.
     * @param pData  Pointer to the source data array.
     * @param count  Number of elements to copy.
     */
    virtual void UpdateAll(const T* pData, size_t count) = 0;

    /**
     * @brief Updates a single element at index @p id.
     * @param id   Element index to update.
     * @param data New element value.
     */
    virtual void UpdateAt(size_t id, const T& data) = 0;
};

/**
 * @brief Concept satisfied when @p Derived is a @ref BufferStorage<T> subclass.
 * @tparam T       Element type.
 * @tparam Derived Concrete storage class to test.
 */
template <typename T, typename Derived>
concept BufferStorageConcept = std::derived_from<Derived, BufferStorage<T>>;

/**
 * @brief Stores exactly one element — suitable for constant buffers with a single struct.
 * @tparam T Element type.
 */
template <typename T>
class WholeBufferStorage : public BufferStorage<T> {
    T m_data; /**< @brief Single-element CPU-side copy. */
public:
    /** @brief Constructs the storage and default-initialises the single element. */
    WholeBufferStorage(Buffer<T>& buffer) : BufferStorage<T>(buffer) {}

    virtual T* GetData() override {
        return &m_data;
    }
    virtual size_t GetDataSize() const override {
        return 1;
    }

    /** @brief Asserts that @p count is 1, then copies the single element. */
    virtual void UpdateAll(const T* pData, size_t count) override {
        assert(count == 1);
        m_data = *pData;
    }
    /** @brief Asserts that @p id is 0, then stores the element. */
    virtual void UpdateAt(size_t id, const T& data) override {
        assert(id == 0);
        m_data = data;
    }
};

/**
 * @brief Stores an arbitrary-length array of elements backed by @c std::vector.
 *
 * Grows dynamically as elements are added.
 *
 * @tparam T Element type.
 */
template <typename T>
class VectorBufferStorage : public BufferStorage<T> {
    std::vector<T> m_data{}; /**< @brief CPU-side element array. */

public:
    /** @brief Constructs an empty vector storage. */
    VectorBufferStorage(Buffer<T>& buffer) : BufferStorage<T>(buffer) {}

    virtual T* GetData() override {
        return m_data.data();
    }
    virtual size_t GetDataSize() const override {
        return m_data.size();
    }

    /** @brief Resizes the vector if @p count exceeds its current size, then copies all elements. */
    virtual void UpdateAll(const T* pData, size_t count) override {
        if (count > m_data.size()) {
            m_data.resize(count);
        }
        for (size_t i{}; i < count; ++i) {
            m_data[i] = pData[i];
        }
    }

    /** @brief Grows the vector to accommodate index @p id if necessary, then stores the element. */
    virtual void UpdateAt(size_t id, const T& data) override {
        if (id >= m_data.size()) {
            m_data.resize(id + 1);
        }
        m_data[id] = data;
    }
};
