#pragma once

#include "Headers.h"
#include "FencedQueue.h"

#include <list>

class Device;
class GPUResource;

// based on https://www.codeproject.com/Articles/1094799/Implementing-Dynamic-Resources-with-Direct-D
struct MemorySegment {
	size_t offset{};
	size_t size{};
};
class FenceBasedRawRingBuffer : public FencedQueue<MemorySegment> {
	size_t m_capacity{};
	size_t m_size{};

	size_t m_head{};
	size_t m_tail{};

	size_t m_currFrameSize{};

public:
	FenceBasedRawRingBuffer() = delete;
	FenceBasedRawRingBuffer(size_t numFrames, size_t capacity);

	size_t GetCapacity() const {
		return m_capacity;
	}
	size_t GetSize() const {
		return m_size;
	}
	bool IsFull() const {
		return GetSize() == GetCapacity();
	};
	bool IsEmpty() const {
		return !GetSize();
	};

	bool Allocate(size_t size, size_t& offset);

protected:
	virtual MemorySegment ProduceForPush() override;

	virtual void BeforePop(const MemorySegment& forPop) override;
};

// Constant blocks must be multiples of 16 constants @ 16 bytes each
#define DEFAULT_ALIGN 256

struct DynamicAllocation {
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

class GPURingBuffer : public FenceBasedRawRingBuffer {
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