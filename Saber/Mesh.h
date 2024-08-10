#pragma once

#include "Headers.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

#include "CommandQueue.h"
#include "CommandList.h"
#include "GPUResource.h"
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

struct BufferData {
    void* data{};
    size_t count{};
    size_t size{};
};

class Mesh {
    std::vector<std::shared_ptr<GPUResource>> m_pBuffers{};
    std::vector<D3D12_VERTEX_BUFFER_VIEW> m_bufferViews{};

    std::shared_ptr<GPUResource> m_pIndexBuffer{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
    
    size_t m_indicesCount{};

public:
    Mesh() = delete;
    Mesh(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    );
    Mesh(
        const std::wstring& filename,
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    );
    Mesh(
        const std::wstring& filename,
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        std::filesystem::path& filepath
    );

    void AddVertexBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const BufferData& bufferData
    );

    void AddIndexBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const BufferData& bufferData,
        DXGI_FORMAT indexFormat
    );

    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView() const;

    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferViews() const;

    size_t GetVertexBuffersCount() const;

    const D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView() const;

    size_t GetIndicesCount() const;

private:
    void InitFromMeshData(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const MeshData& meshData
    );

    std::shared_ptr<GPUResource> CreateBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
        const BufferData& bufferData
    );
};