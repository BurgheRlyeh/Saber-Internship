#ifndef _INDIRECT_COMMAND_BUFFER
#define _INDIRECT_COMMAND_BUFFER
// better protection than pragma once
#include "Headers.h"
#include <vector>

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

struct SimpleIndirectCommand
{
    D3D12_GPU_VIRTUAL_ADDRESS cbv;
    D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
};

template<typename CommandType>
class IndirectCommandBuffer : public GPUResource {
private:
    uint32_t m_bufferSize;
    std::vector<D3D12_INDIRECT_ARGUMENT_DESC> argumentDescs;
    D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc;
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

    std::vector<CommandType>&& GetCpuBuffer() {
        std::vector<CommandType> tmpBuffer; 
        tmpBuffer.reserve(m_bufferSize);
        return std::move(tmpBuffer);
    };
    void FillArgumentDescs() {
        constexpr if (std::is_same<CommandType, SimpleIndirectCommand>)
        {
            argumentDescs.emplace_back(.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW, .RootParameterIndex = 0);
            argumentDescs.emplace_back(.Type = D3D12_DRAW_INDEXED_ARGUMENTS);
            argumentDescs.emplace_back(.Type = D3D12_INDEX_BUFFER_VIEW);
            argumentDescs.emplace_back(.Type = D3D12_VERTEX_BUFFER_VIEW, .VertexBuffer.Slot = 0);

            commandSignatureDesc.pArgumentDescs = argumentDescs;
            commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
            commandSignatureDesc.ByteStride = sizeof(CommandType);
        }
    }
};



#endif
