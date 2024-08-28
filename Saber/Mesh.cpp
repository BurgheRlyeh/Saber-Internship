#include "Mesh.h"

Mesh::Mesh(
    const std::wstring& filename,
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const MeshData& meshData
) {
    std::visit([&](const auto& data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, MeshDataVerticesIndices>) {
            InitFromVerticesIndices(pDevice, pAllocator, pCommandQueueCopy, data);
        }
        else if constexpr (std::is_same_v<T, MeshDataGLTF>) {
            InitFromGLTF(pDevice, pAllocator, pCommandQueueCopy, data);
        }
    }, meshData.data);
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

void Mesh::InitFromVerticesIndices(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const MeshDataVerticesIndices& meshData
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

void Mesh::InitFromGLTF(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const MeshDataGLTF& meshData
) {
    GLTFLoader gltfLoader{ meshData.filepath };

    DXGI_FORMAT format = gltfLoader.GetIndicesFormat();
    switch (format)
    {
    case DXGI_FORMAT_R32_UINT:
    {
        std::vector<uint32_t> indices{};
        gltfLoader.GetIndices(indices);
        BufferData indexBufferData{
            .data{ indices.data() },
            .count{ indices.size() },
            .size{ sizeof(indices.front())},
            .format{format}
        };
        AddIndexBuffer(pDevice, pAllocator, pCommandQueueCopy, indexBufferData, format);
    }
    break;
    case DXGI_FORMAT_R16_UINT:
    {
        std::vector<uint16_t> indices{};

        gltfLoader.GetIndices(indices);
        BufferData indexBufferData{
            .data{ indices.data() },
            .count{ indices.size() },
            .size{ sizeof(indices.front())},
            .format{ format }
        };
        AddIndexBuffer(pDevice, pAllocator, pCommandQueueCopy, indexBufferData, format);
    }
    break;
    default:
        assert(0);
        break;
    }


    for (const Attribute& attribute : meshData.attributes) {
        std::vector<float> vertexData{};
        if (!gltfLoader.GetVerticesData(vertexData, attribute.name)) {
            std::stringstream ss;
            ss << "Bad attribute " << attribute.name << " in file " << meshData.filepath;
            throw std::runtime_error(ss.str());
        }

        BufferData vertexBufferData{
            .data{ vertexData.data() },
            .count{ vertexData.size() / (attribute.size / 4) },
            .size{ attribute.size},
            .format{ DXGI_FORMAT_R32_FLOAT }
        };
        AddVertexBuffer(pDevice, pAllocator, pCommandQueueCopy, vertexBufferData);
    }
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
