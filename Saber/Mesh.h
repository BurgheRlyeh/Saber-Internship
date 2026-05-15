/**
 * @file Mesh.h
 * @brief GPU mesh resource (vertex + index buffers) with loaders for raw
 *        vertex/index data and for GLTF/GLB files.
 *
 * A @ref Mesh holds one index buffer and one or more vertex buffers as
 * @c GPUResource objects and exposes D3D12 buffer-view structs for binding.
 * Two construction paths are supported via the @ref Mesh::MeshData variant:
 *  - @ref Mesh::MeshDataIndicesVertices — raw CPU arrays of indices and vertices.
 *  - @ref Mesh::MeshDataGLTF — path to a GLTF/GLB file with an attribute list.
 */
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

/**
 * @brief A single indexed mesh stored on the GPU.
 *
 * Vertex buffers are stored in a Structure-of-Arrays layout: each @ref VertexData
 * entry in @ref MeshDataIndicesVertices or each @ref Attribute in
 * @ref MeshDataGLTF becomes a separate @c ID3D12Resource.
 */
class Mesh {
    std::vector<std::shared_ptr<GPUResource>> m_pBuffers{}; /**< @brief Vertex buffer GPU resources. */
    std::vector<D3D12_VERTEX_BUFFER_VIEW> m_bufferViews{};  /**< @brief Vertex buffer view descriptors. */

    std::shared_ptr<GPUResource> m_pIndexBuffer{};           /**< @brief Index buffer GPU resource. */
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};             /**< @brief Index buffer view descriptor. */

    size_t m_indicesCount{};                                 /**< @brief Number of indices in the index buffer. */

    /** @brief Internal staging helper for raw CPU buffer data. */
    struct BufferData {
        void* data{};   /**< @brief Pointer to source data. */
        size_t count{}; /**< @brief Number of elements. */
        size_t size{};  /**< @brief Element size in bytes. */
    };

public:
    /**
     * @brief Describes a single vertex stream for the raw-data construction path.
     */
    struct VertexData {
        void* data{};                                  /**< @brief Pointer to the vertex attribute array. */
        size_t size{};                                 /**< @brief Per-element byte size. */
        std::function<void(void*, size_t)> handler{};  /**< @brief Optional per-element callback (e.g. AABB accumulator). */
    };

    /**
     * @brief Raw vertex + index data for direct CPU-array construction.
     */
    struct MeshDataIndicesVertices {
        void* indices{};                                       /**< @brief Pointer to the index array. */
        size_t indicesCnt{};                                   /**< @brief Number of indices. */
        size_t indexSize{};                                    /**< @brief Byte size of one index (2 or 4). */
        DXGI_FORMAT indexFormat{};                             /**< @brief DXGI index format (e.g. R16_UINT). */
        size_t verticesCnt{};                                  /**< @brief Number of vertices per stream. */
        const std::initializer_list<VertexData>& verticesData{}; /**< @brief One entry per vertex stream. */
    };

    /**
     * @brief Identifies a single vertex attribute to extract from a GLTF accessor.
     */
    struct Attribute {
        const std::string name{};                              /**< @brief GLTF accessor name (e.g. "POSITION"). */
        const size_t& size{};                                  /**< @brief Expected per-element byte size. */
        std::function<void(void*, size_t)> handler{};          /**< @brief Optional per-element callback. */
    };

    /**
     * @brief GLTF/GLB file path and the set of vertex attributes to extract.
     */
    struct MeshDataGLTF {
        const std::filesystem::path& filepath{};               /**< @brief Path to the .gltf or .glb file. */
        const std::initializer_list<Attribute>& attributes{};  /**< @brief Ordered list of attributes to load. */
    };

    /**
     * @brief Variant holding either a raw-data or a GLTF construction descriptor.
     */
    struct MeshData {
        std::variant<
            MeshDataIndicesVertices,
            MeshDataGLTF
        > data{};

        MeshData() = delete;
        /** @brief Constructs from raw vertex/index data. */
        MeshData(const MeshDataIndicesVertices& vertexIndexData)
            : data(vertexIndexData)
        {}
        /** @brief Constructs from a GLTF descriptor. */
        MeshData(const MeshDataGLTF & gltfData)
            : data(gltfData)
        {}
    };

    Mesh() = delete;

    /**
     * @brief Uploads the mesh to the GPU.
     * @param filename     Debug name for GPU resources.
     * @param pDeviceContext Device context.
     * @param pCommandList   Copy or direct command list for upload commands.
     * @param meshData     Construction data (raw arrays or GLTF).
     */
    Mesh(
        const std::wstring& filename,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const MeshData& meshData
    );

    /**
     * @brief Returns the vertex buffer view for stream @p id.
     * @param id Zero-based vertex stream index (default 0).
     */
    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView(size_t id = 0) const;

    /** @brief Returns a pointer to the first vertex buffer view (for @c IASetVertexBuffers). */
    const D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferViews() const;

    /** @brief Returns the number of vertex streams (SoA buffers). */
    size_t GetVertexBuffersCount() const;

    /** @brief Returns the index buffer view. */
    const D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView() const;

    /** @brief Returns the total number of indices. */
    size_t GetIndicesCount() const;

private:
    /** @brief Initialises the mesh from raw CPU vertex/index arrays. */
    void InitFromVerticesIndices(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const MeshDataIndicesVertices& meshData
    );

    /** @brief Initialises the mesh by parsing a GLTF/GLB file. */
    void InitFromGLTF(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const MeshDataGLTF& meshData
    );

    /** @brief Uploads an index buffer to the GPU and stores its view. */
    void AddIndexBuffer(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const BufferData& bufferData,
        DXGI_FORMAT indexFormat
    );

    /** @brief Uploads a single vertex stream buffer and appends its view. */
    void AddVertexBuffer(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const BufferData& bufferData
    );

    /**
     * @brief Creates a GPU resource, uploads CPU data via an intermediate buffer,
     *        and returns the committed default-heap resource.
     */
    std::shared_ptr<GPUResource> CreateBuffer(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::shared_ptr<CommandList>& pCommandList,
        const BufferData& bufferData
    );
};
