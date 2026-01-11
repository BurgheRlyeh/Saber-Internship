#include "DynamicUploadRingBuffer.h"

#include <stdexcept>

#include "Device.h"
#include "GPUResource.h"

FenceBasedRawRingBuffer::FenceBasedRawRingBuffer(size_t numFrames, size_t capacity) :
    FencedQueue(numFrames),
    m_capacity(capacity)
{}

bool FenceBasedRawRingBuffer::Allocate(size_t size, size_t & offset) {
    if (IsFull()) {
        return false;
    }

    //        tail          head                  
    //        |             |                     
    //  [xxxxx              xxxxxxxxxxxxxxxxxxx]  
    if (m_tail < m_head && m_tail + size > m_head)
        return false;

    //           head             tail        capacity  
    //           |                |           |         
    //  [        xxxxxxxxxxxxxxxxx            ]         
    if (m_head < m_tail && m_tail + size > m_capacity) {
        if (size > m_head)
            return false;

        size += (m_capacity - m_tail);
        m_tail = 0;
    }

    offset = m_tail;
    m_tail += size;
    m_size += size;
    m_currFrameSize += size;

    return true;
}

MemorySegment FenceBasedRawRingBuffer::ProduceForPush() {
    MemorySegment forPush{ m_tail, m_currFrameSize };
    m_currFrameSize = 0;
    return forPush;
}

void FenceBasedRawRingBuffer::BeforePop(const MemorySegment& forPop) {
    assert(forPop.size <= m_size);
    m_size -= forPop.size;
    m_head = forPop.offset;
}

// GPURingBuffer
GPURingBuffer::GPURingBuffer(
    std::shared_ptr<Device> pDevice,
    size_t capacity,
    const RingBufferType& type
) : FenceBasedRawRingBuffer(3, capacity),
    m_cpuVirtualAddress(nullptr),
    m_gpuVirtualAddress(0)
{
    switch (type) {
    case RingBufferType::CPU: {
		m_pBuffer = std::make_shared<GPUResource>(
			L"RingBuffer/Upload" + std::to_wstring(capacity),
			pDevice,
			GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_UPLOAD },
			GPUResource::ResourceDesc{
				CD3DX12_RESOURCE_DESC::Buffer(GetCapacity()),
				D3D12_RESOURCE_STATE_GENERIC_READ
			}
		);
		m_pBuffer->GetD3D12Resource()->Map(0, nullptr, &m_cpuVirtualAddress);
        break;
    }
    case RingBufferType::GPU: {
		m_pBuffer = std::make_shared<GPUResource>(
			L"RingBuffer/Default" + std::to_wstring(capacity),
			pDevice,
			GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
			GPUResource::ResourceDesc{
				CD3DX12_RESOURCE_DESC::Buffer(
					GetCapacity(),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS
			}
		);
		break;
    }
    default:
        throw std::runtime_error("An attempt was made to create a ring buffer with unknown type");
    }

    m_gpuVirtualAddress = m_pBuffer->GetD3D12Resource()->GetGPUVirtualAddress();
}

GPURingBuffer::~GPURingBuffer() {
    Destroy();
}

DynamicAllocation GPURingBuffer::Allocate(size_t size) {
    size_t offset{};
    if (!FenceBasedRawRingBuffer::Allocate(size, offset)) {
        return DynamicAllocation{ nullptr, 0, 0 };
    }

    DynamicAllocation DynAlloc{ m_pBuffer, offset, size };
    DynAlloc.gpuAddress = m_gpuVirtualAddress + offset;
    if (DynAlloc.cpuAddress = m_cpuVirtualAddress) {
        DynAlloc.cpuAddress = reinterpret_cast<char*>(DynAlloc.cpuAddress) + offset;
    }

    return DynAlloc;
}

void GPURingBuffer::Destroy() {
    if (m_pBuffer && m_cpuVirtualAddress) {
        m_pBuffer->GetD3D12Resource()->Unmap(0, nullptr);
    }
    m_cpuVirtualAddress = nullptr;
    m_gpuVirtualAddress = 0;
}

DynamicUploadHeap::DynamicUploadHeap(
    std::shared_ptr<Device> pDevice,
	size_t initialCapacity,
	const RingBufferType& type
) : m_pDevice(pDevice), m_type(type) {
    m_ringBuffers.emplace_back(pDevice, initialCapacity, m_type);
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
        m_ringBuffers.emplace_back(m_pDevice, newCapacity, m_type);
        dynamicAllocation = m_ringBuffers.back().Allocate(alignedSize);
    }

    return dynamicAllocation;
}
 
void DynamicUploadHeap::FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
    auto lastForDeleting = m_ringBuffers.begin();

    for (auto iter = m_ringBuffers.begin(); iter != m_ringBuffers.end(); ++iter) {
        (*iter).FinishFrame(fenceValue, lastCompletedFenceValue);
        if ((*iter).IsEmpty() && iter == lastForDeleting && iter != --m_ringBuffers.end()) {
            ++lastForDeleting;
        }
    }

    m_ringBuffers.erase(m_ringBuffers.begin(), lastForDeleting);
}
