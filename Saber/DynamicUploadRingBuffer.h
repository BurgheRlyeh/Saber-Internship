#pragma once

#include "Headers.h"

#include <deque>
#include <list>

class Device;
class GPUResource;

// based on https://www.codeproject.com/Articles/1094799/Implementing-Dynamic-Resources-with-Direct-D
class RingBuffer {
public:
    struct FrameAttribs {
        FrameAttribs(uint64_t fenceValue, size_t offset, size_t size) :
            fenceValue(fenceValue),
            offset(offset),
            size(size)
        {}

        uint64_t fenceValue;
        size_t offset;
        size_t size;
    };

private:
    std::deque<FrameAttribs> m_completedFramesAttribs{};
    size_t m_head{};
    size_t m_tail{};
    size_t m_capacity{};
    size_t m_size{};
    size_t m_currFrameSize{};

public:
    RingBuffer() = delete;
    RingBuffer(size_t capacity);

    bool Allocate(size_t size, size_t& offset);

    void FinishCurrentFrame(uint64_t fenceValue);

    void ReleaseCompletedFrames(uint64_t completedFenceValue);

    size_t GetCapacity() const { return m_capacity; }
    size_t GetSize() const { return m_size; }
    bool IsFull() const { return GetSize() == GetCapacity(); };
    bool IsEmpty() const { return !GetSize(); };
};

// Constant blocks must be multiples of 16 constants @ 16 bytes each
#define DEFAULT_ALIGN 256

struct DynamicAllocation {
    DynamicAllocation() = default;
    DynamicAllocation(
        std::shared_ptr<GPUResource> pBuffer,
        size_t offset,
        size_t size
    ) : pBuffer(pBuffer), offset(offset), size(size)
    {}

    std::shared_ptr<GPUResource> pBuffer{};   // The D3D buffer associated with this memory.
    size_t offset{};                                    // Offset from start of buffer resource
    size_t size{};			                            // Reserved size of this allocation
    void* cpuAddress{};			                        // The CPU-writeable address
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress{};         	// The GPU-visible address
};

enum class RingBufferType : size_t {
	CPU = 0,
	GPU = 1,
	Count = 2
};

class GPURingBuffer : public RingBuffer {
    void* m_cpuVirtualAddress;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuVirtualAddress;
    std::shared_ptr<GPUResource> m_pBuffer{};

public:
    GPURingBuffer() = delete;
    GPURingBuffer(
        std::shared_ptr<Device> pDevice,
		size_t capacity,
		const RingBufferType& type
    );

    ~GPURingBuffer();

    DynamicAllocation Allocate(size_t size);

private:
    void Destroy();
};

class DynamicUploadHeap {
    std::shared_ptr<Device> m_pDevice{};
    const RingBufferType m_type;
    std::list<GPURingBuffer> m_ringBuffers{};

public:
    DynamicUploadHeap() = delete;
    DynamicUploadHeap(
        std::shared_ptr<Device> pDevice,
		size_t initialCapacity,
		const RingBufferType& type
    );

    DynamicAllocation Allocate(size_t size, size_t alignment = DEFAULT_ALIGN);

    void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue);

};