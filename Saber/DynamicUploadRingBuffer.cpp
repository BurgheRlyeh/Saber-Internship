#include "DynamicUploadRingBuffer.h"

RingBuffer::RingBuffer(size_t capacity)
    : m_completedFramesAttribs(0, FrameAttribs(0, 0, 0))
    , m_capacity(capacity)
{}

bool RingBuffer::Allocate(size_t size, size_t & offset) {
    if (IsFull()) {
        return false;
    }

    if (m_head <= m_tail) {
        if (m_tail + size <= GetCapacity()) {
            offset = m_tail;

            m_tail += size;
            m_size += size;
            m_currFrameSize += size;

            return true;
        }
        else if (size <= m_head) {
            offset = 0;

            size_t addSize{ (GetCapacity() - m_tail) + size };
            m_tail = size;
            m_size += addSize;
            m_currFrameSize += addSize;

            return true;
        }
    }
    else if (m_tail + size <= m_head) {
        offset = m_tail;

        m_tail += size;
        m_size += size;
        m_currFrameSize += size;

        return true;
    }

    return false;
}

void RingBuffer::FinishCurrentFrame(uint64_t fenceValue) {
    m_completedFramesAttribs.emplace_back(fenceValue, m_tail, m_currFrameSize);
    m_currFrameSize = 0;
}

void RingBuffer::ReleaseCompletedFrames(uint64_t completedFenceValue) {
    while (!m_completedFramesAttribs.empty() && m_completedFramesAttribs.front().fenceValue <= completedFenceValue) {
        const FrameAttribs& oldestFrameTail{ m_completedFramesAttribs.front() };
        assert(oldestFrameTail.size <= m_size);

        m_size -= oldestFrameTail.size;
        m_head = oldestFrameTail.offset;

        m_completedFramesAttribs.pop_front();
    }
}

GPURingBuffer::GPURingBuffer(
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    size_t capacity,
    bool isCPUAccessable
) : RingBuffer(capacity),
    m_cpuVirtualAddress(nullptr),
    m_gpuVirtualAddress(0)
{

    if (isCPUAccessable) {
        m_pBuffer = std::make_shared<GPUResource>(
            pAllocator,
            GPUResource::HeapData{ D3D12_HEAP_TYPE_UPLOAD },
            GPUResource::ResourceData{
                CD3DX12_RESOURCE_DESC::Buffer(
                    GetCapacity(),
                    D3D12_RESOURCE_FLAG_NONE
                ),
                D3D12_RESOURCE_STATE_GENERIC_READ
            }
        );

        m_pBuffer->GetResource()->Map(0, nullptr, &m_cpuVirtualAddress);
    }
    else {
        m_pBuffer = std::make_shared<GPUResource>(
            pAllocator,
            GPUResource::HeapData{ D3D12_HEAP_TYPE_DEFAULT },
            GPUResource::ResourceData{
                CD3DX12_RESOURCE_DESC::Buffer(
                    GetCapacity(),
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                ),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            }
        );
    }

    m_gpuVirtualAddress = m_pBuffer->GetResource()->GetGPUVirtualAddress();
}

GPURingBuffer::~GPURingBuffer() {
    Destroy();
}

DynamicAllocation GPURingBuffer::Allocate(size_t size) {
    size_t offset{};
    if (!RingBuffer::Allocate(size, offset)) {
        return DynamicAllocation(nullptr, 0, 0);
    }

    DynamicAllocation DynAlloc(m_pBuffer, offset, size);
    DynAlloc.gpuAddress = m_gpuVirtualAddress + offset;
    if (DynAlloc.cpuAddress = m_cpuVirtualAddress) {
        DynAlloc.cpuAddress = reinterpret_cast<char*>(DynAlloc.cpuAddress) + offset;
    }

    return DynAlloc;
}

void GPURingBuffer::Destroy() {
    if (m_pBuffer && m_cpuVirtualAddress) {
        m_pBuffer->GetResource()->Unmap(0, nullptr);
    }
    m_cpuVirtualAddress = nullptr;
    m_gpuVirtualAddress = 0;
}

DynamicUploadHeap::DynamicUploadHeap(Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, size_t initialCapacity, bool isCPUAccessible)
    : m_pAllocator(pAllocator)
    , m_isCPUAccessible(isCPUAccessible)
{
    m_ringBuffers.emplace_back(m_pAllocator, initialCapacity, m_isCPUAccessible);
}

DynamicAllocation DynamicUploadHeap::Allocate(size_t size, size_t alignment) {
    const size_t alignmentMask{ alignment - 1 };
    assert((alignmentMask & alignment) == 0);

    const size_t alignedSize{ (size + alignmentMask) & ~alignmentMask };

    DynamicAllocation dynamicAllocation{ m_ringBuffers.back().Allocate(alignedSize) };
    if (!dynamicAllocation.pBuffer) {
        size_t newCapacity{ m_ringBuffers.back().GetCapacity() << 1 };
        while (newCapacity < alignedSize) {
            newCapacity <<= 1;
        }
        m_ringBuffers.emplace_back(m_pAllocator, newCapacity, m_isCPUAccessible);
        dynamicAllocation = m_ringBuffers.back().Allocate(alignedSize);
    }

    return dynamicAllocation;
}
 
void DynamicUploadHeap::FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
    auto lastForDeleting = m_ringBuffers.begin();

    for (auto iter = m_ringBuffers.begin(); iter != m_ringBuffers.end(); ++iter) {
        GPURingBuffer& RingBuff{ *iter };

        RingBuff.FinishCurrentFrame(fenceValue);
        RingBuff.ReleaseCompletedFrames(lastCompletedFenceValue);

        if (RingBuff.IsEmpty() && iter == lastForDeleting && iter != --m_ringBuffers.end()) {
            ++lastForDeleting;
        }
    }

    m_ringBuffers.erase(m_ringBuffers.begin(), lastForDeleting);
}
