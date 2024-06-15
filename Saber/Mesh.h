#pragma once

#include "Headers.h"

#include "CommandQueue.h"

class CommandQueue;

struct MeshData {
    // vertices data
    void* vertices{};
    size_t verticesCnt{};
    size_t vertexSize{};
    // indices data
    void* indices{};
    size_t indicesCnt{};
    size_t indexSize{};
    DXGI_FORMAT indexFormat{};
};

class Mesh {
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pVertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_pIndexBuffer{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};

    size_t m_indicesCount{};

public:
    Mesh() = delete;
    Mesh(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    );

    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView() const;

    const D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView() const;

    size_t GetIndicesCount() const;

private:
    void CreateBufferResource(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
        ID3D12Resource** ppDestinationResource,
        ID3D12Resource** ppIntermediateResource,
        const void* bufferData,
        size_t bufferSize,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
    );
};