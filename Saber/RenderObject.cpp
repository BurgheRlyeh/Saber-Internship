#include "RenderObject.h"

RenderObject::RenderObject(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
    const MeshData& meshData
) {
    m_pMesh = std::make_shared<Mesh>(pDevice, pCommandQueueCopy, meshData);
}

void RenderObject::Update() {

}

void RenderObject::Render(
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
    pCommandListDirect->IASetVertexBuffers(0, 1, m_pMesh->GetVertexBufferView());
    pCommandListDirect->IASetIndexBuffer(m_pMesh->GetIndexBufferView());

    pCommandListDirect->RSSetViewports(1, &viewport);
    pCommandListDirect->RSSetScissorRects(1, &scissorRect);

    pCommandListDirect->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);

    // Update the MVP matrix
    DirectX::XMMATRIX mvpMatrix{ DirectX::XMMatrixMultiply(m_modelMatrix, viewProjectionMatrix) };
    pCommandListDirect->SetGraphicsRoot32BitConstants(0, sizeof(DirectX::XMMATRIX) / 4, &mvpMatrix, 0);

    pCommandListDirect->DrawIndexedInstanced(m_pMesh->GetIndicesCount(), 1, 0, 0, 0);
}
