#pragma once

#include "Headers.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <initializer_list>
#include <variant>

#include "DeviceContext.h"
#include "CommandList.h"
#include "GPUResource.h"

class CommandList;
class DeviceContext;
class GPUResource;

class Mesh {
    std::vector<std::shared_ptr<GPUResource>> m_pBuffers{};
    std::vector<D3D12_VERTEX_BUFFER_VIEW> m_bufferViews{};

    std::shared_ptr<GPUResource> m_pIndexBuffer{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
    
    size_t m_indicesCount{};

    struct BufferData {
        void* data{};
        size_t count{};
        size_t size{};
    };

public:
    struct VertexData {
        void* data{};
        size_t size{};
        std::function<void(void*, size_t)> handler{};
    };
    struct MeshDataIndicesVertices {
        // indices data
        void* indices{};
        size_t indicesCnt{};
        size_t indexSize{};
        DXGI_FORMAT indexFormat{};
        // vertices data
        size_t verticesCnt{};
        const std::initializer_list<VertexData>& verticesData{};
    };

    struct Attribute {
        const std::string name{};
        const size_t& size{};
        std::function<void(void*, size_t)> handler{};
    };

    struct MeshDataGLTF {
        const std::filesystem::path& filepath{};
        const std::initializer_list<Attribute>& attributes{};
    };

    struct MeshData {
        std::variant<
            MeshDataIndicesVertices,
            MeshDataGLTF
        > data{};

        MeshData() = delete;
        MeshData(const MeshDataIndicesVertices& vertexIndexData)
            : data(vertexIndexData)
        {}
        MeshData(const MeshDataGLTF & gltfData)
            : data(gltfData)
        {}
    };

    Mesh() = delete;
    Mesh(
        const std::wstring& filename,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const MeshData& meshData
    );

    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView(size_t id = 0) const;

    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferViews() const;

    size_t GetVertexBuffersCount() const;

    const D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView() const;

    size_t GetIndicesCount() const;

private:
    void InitFromVerticesIndices(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const MeshDataIndicesVertices& meshData
    );
    void InitFromGLTF(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const MeshDataGLTF& meshData
    );

    void AddIndexBuffer(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const BufferData& bufferData,
        DXGI_FORMAT indexFormat
    );

    void AddVertexBuffer(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const BufferData& bufferData
    );

    std::shared_ptr<GPUResource> CreateBuffer(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const BufferData& bufferData
    );
};