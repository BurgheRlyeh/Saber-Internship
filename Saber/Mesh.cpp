#include "GLTFLoader.h"

#include "Mesh.h"

Mesh::Mesh(
    const std::wstring& filename,
    std::shared_ptr<DeviceContext> pDeviceContext,
    const std::shared_ptr<CommandList>& pCommandList,
    const MeshData& meshData
) {
    std::visit([&](const auto& data) {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, MeshDataIndicesVertices>) {
            InitFromVerticesIndices(filename, pDeviceContext, pCommandList, data);
        }
        else if constexpr (std::is_same_v<T, MeshDataGLTF>) {
            InitFromGLTF(filename, pDeviceContext, pCommandList, data);
        }
    }, meshData.data);
}

const D3D12_VERTEX_BUFFER_VIEW* Mesh::GetVertexBufferView(size_t id) const {
    assert(id < GetVertexBuffersCount());
    return &m_bufferViews[id];
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
    const std::wstring& name,
    std::shared_ptr<DeviceContext> pDeviceContext,
    const std::shared_ptr<CommandList>& pCommandList,
    const MeshDataIndicesVertices& meshData
) {
    AddIndexBuffer(
        name + L"/IndexBuffer",
        pDeviceContext,
        pCommandList,
        BufferData{
            .data{ meshData.indices },
            .count{ meshData.indicesCnt },
            .size{ meshData.indexSize }
        },
        meshData.indexFormat
    );

    size_t i{};
    for (const VertexData& vertexData : meshData.verticesData) {
        if (vertexData.handler) {
            vertexData.handler(vertexData.data, vertexData.size);
        }
        AddVertexBuffer(
            name + L"/VertexBuffer" + std::to_wstring(i++),
            pDeviceContext,
            pCommandList,
            BufferData{
                .data{ vertexData.data },
                .count{ meshData.verticesCnt },
                .size{ vertexData.size }
            }
        );
    }
}

void Mesh::InitFromGLTF(
    const std::wstring& name,
    std::shared_ptr<DeviceContext> pDeviceContext,
    const std::shared_ptr<CommandList>& pCommandList,
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
            .size{ sizeof(indices.front())}
        };
        AddIndexBuffer(name + L"/IndexBuffer", pDeviceContext, pCommandList, indexBufferData, format);
    }
    break;
    case DXGI_FORMAT_R16_UINT:
    {
        std::vector<uint16_t> indices{};

        gltfLoader.GetIndices(indices);
        BufferData indexBufferData{
            .data{ indices.data() },
            .count{ indices.size() },
            .size{ sizeof(indices.front())}
        };
        AddIndexBuffer(name + L"/IndexBuffer", pDeviceContext, pCommandList, indexBufferData, format);
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

        size_t verticesCnt{ vertexData.size() / (attribute.size / 4) };
        if (attribute.handler) {
            attribute.handler(vertexData.data(), verticesCnt);
        }

        BufferData vertexBufferData{
            .data{ vertexData.data() },
            .count{ verticesCnt },
            .size{ attribute.size }
        };
        std::wstring attributeName(attribute.name.begin(), attribute.name.end());
        AddVertexBuffer(name + L"/VertexBuffer/" + attributeName, pDeviceContext, pCommandList, vertexBufferData);
    }
}

void Mesh::AddIndexBuffer(
    const std::wstring& indexBufferName,
    std::shared_ptr<DeviceContext> pDeviceContext,
    const std::shared_ptr<CommandList>& pCommandList,
    const BufferData& bufferData,
    DXGI_FORMAT indexFormat
) {
    m_indicesCount = bufferData.count;

    m_pIndexBuffer = CreateBuffer(
        indexBufferName,
        pDeviceContext,
        pCommandList,
        bufferData
    );

    m_indexBufferView = {
        .BufferLocation{ m_pIndexBuffer->GetD3D12Resource()->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<UINT>(bufferData.size * bufferData.count) },
        .Format{ indexFormat }
    };
}

void Mesh::AddVertexBuffer(
    const std::wstring& vertexBufferName,
    std::shared_ptr<DeviceContext> pDeviceContext,
    const std::shared_ptr<CommandList>& pCommandList,
    const BufferData& bufferData
) {
    m_pBuffers.push_back(CreateBuffer(
        vertexBufferName,
        pDeviceContext,
        pCommandList,
        bufferData
    ));

    m_bufferViews.push_back(D3D12_VERTEX_BUFFER_VIEW{
        .BufferLocation{ m_pBuffers.back()->GetD3D12Resource().Get()->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<UINT>(bufferData.size * bufferData.count) },
        .StrideInBytes{ static_cast<UINT>(bufferData.size) }
        });
}

std::shared_ptr<GPUResource> Mesh::CreateBuffer(
    const std::wstring& bufferName,
    std::shared_ptr<DeviceContext> pDeviceContext,
    const std::shared_ptr<CommandList>& pCommandList,
    const BufferData& bufferData
) {
    size_t bufferSize{ bufferData.count * bufferData.size };

    std::shared_ptr<GPUResource> pBuffer{ std::make_shared<GPUResource>(
        bufferName,
        pDeviceContext->GetDevice(),
        GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
        GPUResource::ResourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(bufferSize) }
    ) };

    D3D12_SUBRESOURCE_DATA subresourceData{
        .pData{ bufferData.data },
        .RowPitch{ static_cast<LONG_PTR>(bufferSize) },
        .SlicePitch{ subresourceData.RowPitch }
    };

    std::shared_ptr<GPUResource> pIntermediate{ pBuffer->CreateIntermediate(pDeviceContext->GetDevice()) };
    pBuffer->UpdateSubresources(
		pCommandList,
		pIntermediate,
        &subresourceData
    );
    pCommandList->KeepAlive(pIntermediate);

    return pBuffer;
}
