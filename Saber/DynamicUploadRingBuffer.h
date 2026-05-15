/**
 * @file DynamicUploadRingBuffer.h
 * @brief Lock-based GPU upload ring buffers for per-frame transient allocations.
 *
 * Provides three layers:
 *  - @ref FenceBasedRawRingBuffer — a raw ring-buffer implementation tracking
 *    byte segments with fence-guarded reclamation.
 *  - @ref GPURingBuffer — wraps the raw ring buffer with mapped GPU memory
 *    and returns @ref DynamicAllocation descriptors.
 *  - @ref DynamicUploadHeap — a list of @ref GPURingBuffer instances that
 *    grows on demand when the current buffer is full.
 *
 * @note Based on https://www.codeproject.com/Articles/1094799/Implementing-Dynamic-Resources-with-Direct-D
 */
#pragma once

#include "Headers.h"
#include "FencedQueue.h"

#include <list>

class Device;
class GPUResource;

/** @brief Describes a contiguous byte segment within a ring buffer. */
struct MemorySegment {
    size_t offset{}; /**< @brief Byte offset from the start of the buffer. */
    size_t size{};   /**< @brief Size of the segment in bytes. */
};

/**
 * @brief Thread-safe fence-guarded ring buffer for byte segments.
 *
 * Allocates segments from a fixed-size ring.  When a frame ends, the
 * segments allocated in that frame are pushed into a @ref FencedQueue and
 * released once the GPU reports the corresponding fence value as complete.
 */
class FenceBasedRawRingBuffer : public FencedQueue<MemorySegment> {
    size_t m_capacity{};
    size_t m_size{};

    size_t m_head{};
    size_t m_tail{};

    size_t m_currFrameSize{}; /**< @brief Bytes allocated in the current (not yet finished) frame. */

public:
    FenceBasedRawRingBuffer() = delete;

    /**
     * @brief Constructs the ring buffer.
     * @param numFrames Number of frames-in-flight (queue depth).
     * @param capacity  Total ring-buffer capacity in bytes.
     */
    FenceBasedRawRingBuffer(size_t numFrames, size_t capacity);

    /** @brief Returns the total ring-buffer capacity in bytes. */
    size_t GetCapacity() const {
        return m_capacity;
    }
    /** @brief Returns the number of bytes currently allocated (not yet reclaimed). */
    size_t GetSize() const {
        return m_size;
    }
    /** @brief Returns @c true if the ring buffer is completely full. */
    bool IsFull() const {
        return GetSize() == GetCapacity();
    };
    /** @brief Returns @c true if no bytes are currently allocated. */
    bool IsEmpty() const {
        return !GetSize();
    };

    /**
     * @brief Attempts to allocate @p size bytes from the ring.
     * @param size   Number of bytes to allocate.
     * @param offset Receives the byte offset of the allocation on success.
     * @return @c true if the allocation succeeded; @c false if the ring is too full.
     */
    bool Allocate(size_t size, size_t& offset);

protected:
    /** @brief Snapshot of the current frame's allocated segment for the fence queue. */
    virtual MemorySegment ProduceForPush() override;

    /** @brief Called when a completed frame's segment is popped; reclaims the bytes. */
    virtual void BeforePop(const MemorySegment& forPop) override;
};

/** @brief Alignment in bytes required by constant buffers (256 B). */
#define DEFAULT_ALIGN 256

/**
 * @brief Describes a single transient GPU memory allocation from a ring buffer.
 */
struct DynamicAllocation {
    std::shared_ptr<GPUResource> pBuffer{};   /**< @brief The ring-buffer GPU resource backing this allocation. */
    size_t offset{};                           /**< @brief Byte offset within @ref pBuffer. */
    size_t size{};                             /**< @brief Byte size of this allocation. */
    void* cpuAddress{};                        /**< @brief CPU-mapped write address. */
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress{};    /**< @brief Corresponding GPU virtual address. */
};

/**
 * @brief Distinguishes CPU-visible (upload heap) from GPU-local ring buffers.
 */
enum class RingBufferType : size_t {
    CPU   = 0, /**< @brief Upload heap; CPU writes, GPU reads. */
    GPU   = 1, /**< @brief GPU-local heap; filled via copy commands. */
    Count = 2
};

/**
 * @brief A @ref FenceBasedRawRingBuffer backed by a persistently mapped GPU upload resource.
 *
 * Allocations return @ref DynamicAllocation values carrying both CPU and GPU addresses.
 */
class GPURingBuffer : public FenceBasedRawRingBuffer {
    void* m_cpuVirtualAddress;                    /**< @brief Persistently mapped CPU pointer to the buffer start. */
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuVirtualAddress; /**< @brief GPU virtual address of the buffer start. */
    std::shared_ptr<GPUResource> m_pBuffer{};     /**< @brief Underlying GPU resource. */

public:
    GPURingBuffer() = delete;

    /**
     * @brief Allocates and maps the GPU upload buffer.
     * @param pDevice  Device used to create the resource.
     * @param capacity Total buffer capacity in bytes.
     * @param type     Whether to allocate on the upload (CPU) or default (GPU) heap.
     */
    GPURingBuffer(
        std::shared_ptr<Device> pDevice,
        size_t capacity,
        const RingBufferType& type
    );

    ~GPURingBuffer();

    /**
     * @brief Allocates @p size bytes (aligned to @c DEFAULT_ALIGN) and returns the descriptor.
     * @param size Requested allocation size in bytes.
     * @return Allocation descriptor with both CPU and GPU addresses.
     */
    DynamicAllocation Allocate(size_t size);

private:
    /** @brief Unmaps and releases the GPU resource. */
    void Destroy();
};

/**
 * @brief A growable list of @ref GPURingBuffer instances that expands when full.
 *
 * On each @c Allocate call the first buffer with enough space is used; if all
 * buffers are full a new one is appended to the list.
 */
class DynamicUploadHeap {
    std::shared_ptr<Device> m_pDevice{};
    const RingBufferType m_type;
    std::list<GPURingBuffer> m_ringBuffers{}; /**< @brief Active ring buffers; front is tried first. */

public:
    DynamicUploadHeap() = delete;

    /**
     * @brief Constructs the heap and allocates the first ring buffer.
     * @param pDevice         Device used to create GPU resources.
     * @param initialCapacity Initial ring-buffer capacity in bytes.
     * @param type            Heap type (CPU upload or GPU local).
     */
    DynamicUploadHeap(
        std::shared_ptr<Device> pDevice,
        size_t initialCapacity,
        const RingBufferType& type
    );

    /**
     * @brief Allocates @p size bytes aligned to @p alignment from the heap.
     *
     * Grows the heap by appending a new ring buffer if the current one is too full.
     *
     * @param size      Requested allocation size in bytes.
     * @param alignment Required alignment in bytes (default @ref DEFAULT_ALIGN).
     * @return Allocation descriptor.
     */
    DynamicAllocation Allocate(size_t size, size_t alignment = DEFAULT_ALIGN);

    /**
     * @brief Advances the per-frame fenced release tracking for all ring buffers.
     * @param fenceValue             Fence value signalled for the current frame.
     * @param lastCompletedFenceValue Highest fence value the GPU has completed.
     */
    void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue);
};
