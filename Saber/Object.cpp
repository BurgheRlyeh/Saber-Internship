#include "Object.h"

Object::Object(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const void* vertices, size_t verticesCnt, size_t vertexSize,
    const void* indices, size_t indicesCnt, size_t indexSize, DXGI_FORMAT indexFormat
) : m_indicesCount(indicesCnt) {
    assert(pCommandQueueCopy->GetCommandListType() == D3D12_COMMAND_LIST_TYPE_COPY);

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList{
        pCommandQueueCopy->GetCommandList(pDevice)
    };

    // Upload vertex buffer data
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateVertexBuffer{};
    CreateBufferResource(
        pDevice,
        pCommandList,
        &m_pVertexBuffer,
        &intermediateVertexBuffer,
        verticesCnt,
        vertexSize,
        vertices
    );

    // Create the vertex buffer view
    m_vertexBufferView = {
        .BufferLocation{ m_pVertexBuffer->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<uint32_t>(verticesCnt * vertexSize) },
        .StrideInBytes{ static_cast<uint32_t>(vertexSize) }
    };

    // Upload index buffer data
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateIndexBuffer{};
    CreateBufferResource(
        pDevice,
        pCommandList,
        &m_pIndexBuffer,
        &intermediateIndexBuffer,
        indicesCnt,
        indexSize,
        indices
    );

    // Create index buffer view
    m_indexBufferView = {
        .BufferLocation{ m_pIndexBuffer->GetGPUVirtualAddress() },
        .SizeInBytes{ static_cast<uint32_t>(indicesCnt * indexSize) },
        .Format{ indexFormat },
    };

    pCommandQueueCopy->ExecuteCommandListImmediately(pCommandList);
}

void Object::Update() {

}

void Object::Render(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState,
    Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
    DirectX::XMMATRIX viewProjectionMatrix) const {
    assert(pCommandListDirect->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);

    pCommandListDirect->SetPipelineState(pPipelineState.Get());
    pCommandListDirect->SetGraphicsRootSignature(pRootSignature.Get());

    pCommandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandListDirect->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    pCommandListDirect->IASetIndexBuffer(&m_indexBufferView);

    pCommandListDirect->RSSetViewports(1, &viewport);
    pCommandListDirect->RSSetScissorRects(1, &scissorRect);

    pCommandListDirect->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);

    // Update the MVP matrix
    DirectX::XMMATRIX mvpMatrix{ DirectX::XMMatrixMultiply(m_modelMatrix, viewProjectionMatrix) };
    pCommandListDirect->SetGraphicsRoot32BitConstants(0, sizeof(DirectX::XMMATRIX) / 4, &mvpMatrix, 0);

    pCommandListDirect->DrawIndexedInstanced(m_indicesCount, 1, 0, 0, 0);
}

void Object::CreateBufferResource(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    ID3D12Resource** ppDestinationResource,
    ID3D12Resource** ppIntermediateResource,
    size_t numElements,
    size_t elementSize,
    const void* bufferData,
    D3D12_RESOURCE_FLAGS flags
) {
    size_t bufferSize{ numElements * elementSize };

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
