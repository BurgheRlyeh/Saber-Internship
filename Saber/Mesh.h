#pragma once

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/Deserialize.h>

#include "Headers.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

#include "CommandQueue.h"
#include "CommandList.h"
#include "GLTFLoader.h"

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
    Mesh(
        const std::wstring& filename,
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    );
    Mesh(
        const std::wstring& filename,
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::filesystem::path& filepath
    );

    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView() const;

    const D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView() const;

    size_t GetIndicesCount() const;

private:
    void InitFromMeshData(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    );
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