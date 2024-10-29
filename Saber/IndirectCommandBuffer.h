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
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;

};

template<typename CommandType>
class IndirectCommandBuffer : public GPUResource {
private:
    uint32_t m_bufferSize;
    uint32_t m_Size;
    //std::vector<D3D12_INDIRECT_ARGUMENT_DESC> m_argumentDescs;
    //D3D12_COMMAND_SIGNATURE_DESC m_commandSignatureDesc;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_commandSignature;
public:
    IndirectCommandBuffer(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        unsigned int objectsCount,
        const HeapData& heapData = HeapData{ D3D12_HEAP_TYPE_DEFAULT },
        const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
    ) : GPUResource(pAllocator, heapData, ResourceData{ ResourceData{
            CD3DX12_RESOURCE_DESC::Buffer(AlignForUavCounter(objectsCount * sizeof(CommandType))),
            D3D12_RESOURCE_STATE_COPY_DEST
        }, }, allocationFlags), m_bufferSize(AlignForUavCounter(objectsCount * sizeof(CommandType))), m_Size(objectsCount) {
        GetResource().Get()->SetName(L"CommandBuffer");
        FillArgumentDescs(pDevice, rootSignature);
    }

    uint32_t GetMemSize() { return m_bufferSize; };

    std::vector<CommandType>&& GetCpuBuffer() {
        std::vector<CommandType> tmpBuffer; 
        tmpBuffer.reserve(m_bufferSize);
        return std::move(tmpBuffer);
    }
    void FillArgumentDescs(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature) {
        if constexpr (std::is_same<CommandType, SimpleIndirectCommand>::value)  
        {
            std::vector<D3D12_INDIRECT_ARGUMENT_DESC> m_argumentDescs;
            m_argumentDescs.push_back({ .Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW, .ConstantBufferView = {.RootParameterIndex = 1} });
            m_argumentDescs.push_back({ .Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW });
            m_argumentDescs.push_back({ .Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW, .VertexBuffer = {.Slot = 0} });
            m_argumentDescs.push_back({ .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED });

            D3D12_COMMAND_SIGNATURE_DESC m_commandSignatureDesc = {};
            m_commandSignatureDesc.pArgumentDescs = m_argumentDescs.data();
            m_commandSignatureDesc.NumArgumentDescs = m_argumentDescs.size();
            m_commandSignatureDesc.ByteStride = sizeof(CommandType);

            ThrowIfFailed(pDevice->CreateCommandSignature(&m_commandSignatureDesc, rootSignature.Get(), IID_PPV_ARGS(&m_commandSignature)));
            //ThrowIfFailed(pDevice->CreateCommandSignature(&m_commandSignatureDesc, NULL, IID_PPV_ARGS(&m_commandSignature)));
        }

    }

    void DrawIndirect(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList )
    {
        pCommandList->ExecuteIndirect(
            m_commandSignature.Get(),
            m_Size,
            GetResource().Get(),
            0,
            nullptr,
            0);
        //pCommandListDirect->ExecuteIndirect(
        //        m_commandSignature.Get(),
        //        TriangleCount,
        //        m_commandBuffer.Get(),
        //        CommandSizePerFrame * m_frameIndex,
        //        nullptr,
        //        0);
    }
};



#endif
