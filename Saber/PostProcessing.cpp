#include "PostProcessing.h"

void PostProcessing::Render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, D3D12_VIEWPORT viewport, D3D12_RECT rect, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT& rootParameterIndex)> outerRootParametersSetter) {
    pCommandListDirect->SetPipelineState(m_pPipelineState.Get());
    pCommandListDirect->SetGraphicsRootSignature(m_pRootSignatureResource->pRootSignature.Get());

    pCommandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    pCommandListDirect->RSSetViewports(1, &viewport);
    pCommandListDirect->RSSetScissorRects(1, &rect);

    pCommandListDirect->OMSetRenderTargets(1, &renderTargetView, FALSE, nullptr);

    RenderJob(pCommandListDirect);

    UINT rootParameterIndex{};
    outerRootParametersSetter(pCommandListDirect, rootParameterIndex);
    InnerRootParametersSetter(pCommandListDirect, rootParameterIndex);

    pCommandListDirect->DrawInstanced(
        GetIndexCountPerInstance(),
        GetInstanceCount(),
        0, 0
    );
}

UINT PostProcessing::GetIndexCountPerInstance() const {
    return 3;
}

UINT PostProcessing::GetInstanceCount() const {
    return 1;
}
