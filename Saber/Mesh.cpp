#include "Mesh.h"

Mesh::Mesh(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const MeshData& meshData
) {
    InitFromMeshData(pDevice, pAllocator, pCommandQueueCopy, meshData);
}

Mesh::Mesh(
    const std::wstring& filename,
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const MeshData& meshData
) : Mesh(pDevice, pAllocator, pCommandQueueCopy, meshData)
{}

Mesh::Mesh(
    const std::wstring& filename,
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    std::filesystem::path& filepath
) {
    std::vector<uint32_t> indices{};
    std::vector<VertexPositionColor> vertices{};
    GLTFLoader::LoadMeshFromGLTF(filepath, indices, vertices);
    MeshData meshData{
        // vertices data
        .vertices{ vertices.data() },
        .verticesCnt{ vertices.size() },
        .vertexSize{ sizeof(vertices[0])},
        // indices data
        .indices{ indices.data() },
        .indicesCnt{ indices.size() },
        .indexSize{ sizeof(indices[0]) },
        .indexFormat{ DXGI_FORMAT_R32_UINT }
    };

    InitFromMeshData(pDevice, pAllocator, pCommandQueueCopy, meshData);
}

void Mesh::AddVertexBuffer(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const BufferData& bufferData
) {
    m_pBuffers.push_back(CreateBuffer(
        pDevice,
        pAllocator,
        pCommandQueueCopy,
        bufferData
    ));

    m_bufferViews.push_back(D3D12_VERTEX_BUFFER_VIEW{
        .BufferLocation{ m_pBuffers.back()->GetResource().Get()->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<UINT>(bufferData.size * bufferData.count) },
        .StrideInBytes{ static_cast<UINT>(bufferData.size) }
    });
}

void Mesh::AddIndexBuffer(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const BufferData& bufferData,
    DXGI_FORMAT indexFormat
) {
    m_indicesCount = bufferData.count;

    m_pIndexBuffer = CreateBuffer(
        pDevice,
        pAllocator,
        pCommandQueueCopy,
        bufferData
    );

    m_indexBufferView = {
        .BufferLocation{ m_pIndexBuffer->GetResource()->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<UINT>(bufferData.size * bufferData.count) },
        .Format{ indexFormat }
    };
}

const D3D12_VERTEX_BUFFER_VIEW* Mesh::GetVertexBufferView() const {
    return GetVertexBuffersCount() == 1 ? &m_bufferViews.front() : nullptr;
}

const D3D12_VERTEX_BUFFER_VIEW* Mesh::GetVertexBufferViews() const {
    return m_bufferViews.data();
}

size_t Mesh::GetVertexBuffersCount() const {
    return m_pBuffers.size();
}

const D3D12_INDEX_BUFFER_VIEW* Mesh::GetIndexBufferView() const {
    return &m_indexBufferView;
}

size_t Mesh::GetIndicesCount() const {
    return m_indicesCount;
}

void Mesh::InitFromMeshData(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const MeshData& meshData
) {
    AddVertexBuffer(
        pDevice,
        pAllocator,
        pCommandQueueCopy,
        BufferData{
            .data{ meshData.vertices },
            .count{ meshData.verticesCnt },
            .size{ meshData.vertexSize }
        }
    );

    AddIndexBuffer(
        pDevice,
        pAllocator,
        pCommandQueueCopy,
        BufferData{
            .data{ meshData.indices },
            .count{ meshData.indicesCnt },
            .size{ meshData.indexSize }
        },
        meshData.indexFormat
    );
}

std::shared_ptr<GPUResource> Mesh::CreateBuffer(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const BufferData& bufferData
) {
    size_t bufferSize{ bufferData.count * bufferData.size };

    std::shared_ptr<GPUResource> pBuffer{ std::make_shared<GPUResource>(
        pAllocator,
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_COPY_DEST
    ) };

    GPUResource intermediateBuffer{
        pAllocator,
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ
    };

    D3D12_SUBRESOURCE_DATA subresourceData{
        .pData{ bufferData.data },
        .RowPitch{ static_cast<LONG_PTR>(bufferSize) },
        .SlicePitch{ subresourceData.RowPitch }
    };

    std::shared_ptr<CommandList> pCommandList{
        pCommandQueueCopy->GetCommandList(pDevice)
    };

    UpdateSubresources(
        pCommandList->m_pCommandList.Get(),
        pBuffer->GetResource().Get(),
        intermediateBuffer.GetResource().Get(),
        0,
        0,
        1,
        &subresourceData
    );

    pCommandQueueCopy->ExecuteCommandListImmediately(pCommandList);

    return pBuffer;
}
