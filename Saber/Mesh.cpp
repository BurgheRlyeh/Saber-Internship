#include "Mesh.h"

Mesh::Mesh(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    , std::shared_ptr<CommandQueue> const& pCommandQueueCopy
    , const MeshData& meshData
) {
    InitFromMeshData(pDevice, pCommandQueueCopy, meshData);
}

Mesh::Mesh(
    const std::wstring& filename
    , Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    , std::shared_ptr<CommandQueue> const& pCommandQueueCopy
    , const MeshData& meshData
) : Mesh(pDevice, pCommandQueueCopy, meshData)
{}

Mesh::Mesh(
    const std::wstring& filename,
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
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

    InitFromMeshData(pDevice, pCommandQueueCopy, meshData);
}

const D3D12_VERTEX_BUFFER_VIEW* Mesh::GetVertexBufferView() const {
    return &m_vertexBufferView;
}

const D3D12_INDEX_BUFFER_VIEW* Mesh::GetIndexBufferView() const {
    return &m_indexBufferView;
}

size_t Mesh::GetIndicesCount() const {
    return m_indicesCount;
}

void Mesh::InitFromMeshData(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    , std::shared_ptr<CommandQueue> const& pCommandQueueCopy
    , const MeshData& meshData
)
{
    assert(pCommandQueueCopy->GetCommandListType() == D3D12_COMMAND_LIST_TYPE_COPY);

    m_indicesCount = meshData.indicesCnt;

    std::shared_ptr<CommandList> commandList{
        pCommandQueueCopy->GetCommandList(pDevice)
    };

    // Upload vertex buffer data
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateVertexBuffer{};
    CreateBufferResource(
        pDevice,
        commandList->m_pCommandList,
        &m_pVertexBuffer,
        &intermediateVertexBuffer,
        meshData.vertices,
        meshData.verticesCnt * meshData.vertexSize
    );

    // Create the vertex buffer view
    m_vertexBufferView = {
        .BufferLocation{ m_pVertexBuffer->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<UINT>(meshData.verticesCnt * meshData.vertexSize) },
        .StrideInBytes{ static_cast<UINT>(meshData.vertexSize) }
    };

    // Upload index buffer data
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateIndexBuffer{};
    CreateBufferResource(
        pDevice,
        commandList->m_pCommandList,
        &m_pIndexBuffer,
        &intermediateIndexBuffer,
        meshData.indices,
        m_indicesCount * meshData.indexSize
    );

    // Create index buffer view
    m_indexBufferView = {
        .BufferLocation{ m_pIndexBuffer->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<UINT>(m_indicesCount * meshData.indexSize) },
        .Format{ meshData.indexFormat },
    };

    pCommandQueueCopy->ExecuteCommandListImmediately(commandList);
}

void Mesh::CreateBufferResource(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    ID3D12Resource** ppDestinationResource,
    ID3D12Resource** ppIntermediateResource,
    const void* bufferData,
    size_t bufferSize,
    D3D12_RESOURCE_FLAGS flags
) {
    ThrowIfFailed(pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(ppDestinationResource)
    ));

    if (!bufferData)
        return;

    ThrowIfFailed(pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(ppIntermediateResource)
    ));

    D3D12_SUBRESOURCE_DATA subresourceData{
        .pData{ bufferData },
        .RowPitch{ static_cast<LONG_PTR>(bufferSize) },
        .SlicePitch{ subresourceData.RowPitch }
    };

    UpdateSubresources(
        pCommandList.Get(),
        *ppDestinationResource,
        *ppIntermediateResource,
        0,
        0,
        1,
        &subresourceData
    );
}
