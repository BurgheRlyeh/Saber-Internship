#ifndef _INDIRECT_COMMAND_BUFFER
#define _INDIRECT_COMMAND_BUFFER
// better protection than pragma once
#include "Headers.h"

#include "GPUResource.h"
// From Microsoft's IndirectDraw example
// 
    // We pack the UAV counter into the same buffer as the commands rather than create
    // a separate 64K resource/heap for it. The counter must be aligned on 4K boundaries,
    // so we pad the command buffer (if necessary) such that the counter will be placed
    // at a valid location in the buffer.
static inline UINT AlignForUavCounter(UINT bufferSize)
{
    const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
    return (bufferSize + (alignment - 1)) & ~(alignment - 1);
}

template<typename CommandType>
class IndirectCommandBuffer : public GPUResource {
private:
    uint32_t m_bufferSize;

public:
    IndirectCommandBuffer(
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        unsigned int objectsCount,
        const HeapData& heapData = HeapData{ D3D12_HEAP_TYPE_DEFAULT },
        const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
    ) : m_bufferSize(AlignForUavCounter(objectsCount * sizeof(CommandType))), GPUResource(pAllocator, heapData, ResourceData{ResourceData{
            CD3DX12_RESOURCE_DESC::Buffer(m_bufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST
        }, }, allocationFlags) {}
    uint32_t GetMemSize() { return m_bufferSize; };

};

struct SimpleIndirectCommand
{
    D3D12_GPU_VIRTUAL_ADDRESS cbv;
    D3D12_DRAW_ARGUMENTS drawArguments;
};

#endif
