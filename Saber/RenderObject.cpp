#include "RenderObject.h"

void RenderObject::Update() {

}

void RenderObject::Render(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
    DirectX::XMMATRIX viewProjectionMatrix
) const {
    assert(pCommandListDirect->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);

    pCommandListDirect->SetPipelineState(m_pPipelineState.Get());

    pCommandListDirect->SetGraphicsRootSignature(m_pRootSignatureResource->pRootSignature.Get());

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
