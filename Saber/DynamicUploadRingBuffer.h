#pragma once

#include "Headers.h"

#include <deque>

#include "GPUResource.h"

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
        Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer,
        size_t offset,
        size_t size
    ) : pBuffer(pBuffer), offset(offset), size(size)
    {}

    Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer{};   // The D3D buffer associated with this memory.
    size_t offset{};                                    // Offset from start of buffer resource
    size_t size{};			                            // Reserved size of this allocation
    void* cpuAddress{};			                        // The CPU-writeable address
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress{};         	// The GPU-visible address
};

class GPURingBuffer : public RingBuffer {
    void* m_cpuVirtualAddress;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuVirtualAddress;
    std::shared_ptr<GPUResource> m_pBuffer{};

public:
    GPURingBuffer() = delete;
    GPURingBuffer(
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        size_t capacity,
        bool isCPUAccessable
    );

    ~GPURingBuffer();

    DynamicAllocation Allocate(size_t size);

private:
    void Destroy();
};

class DynamicUploadHeap {
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> m_pAllocator{};
    const bool m_isCPUAccessible;
    std::vector<GPURingBuffer> m_ringBuffers{};

public:
    DynamicUploadHeap() = delete;
    DynamicUploadHeap(
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        size_t initialCapacity,
        bool isCPUAccessible
    );

    DynamicAllocation Allocate(size_t size, size_t alignment = DEFAULT_ALIGN);

    void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue);

};