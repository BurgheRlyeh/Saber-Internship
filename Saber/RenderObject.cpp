#include "RenderObject.h"

void RenderObject::InitMaterial(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    const RootSignatureData& rootSignatureData,
    const ShaderData& shaderData,
    PipelineStateData& pipelineStateData
) {
    m_pRootSignatureResource = rootSignatureData.pRootSignatureAtlas->Assign(
        rootSignatureData.rootSignatureFilename,
        pDevice,
        rootSignatureData.pRootSignatureBlob
    );

    m_pVertexShaderResource = shaderData.pShaderAtlas->Assign(shaderData.vertexShaderFilepath);
    m_pPixelShaderResource = shaderData.pShaderAtlas->Assign(shaderData.pixelShaderFilepath);

    pipelineStateData.desc.pRootSignature = m_pRootSignatureResource->pRootSignature.Get();
    pipelineStateData.desc.VS = CD3DX12_SHADER_BYTECODE(m_pVertexShaderResource->pShaderBlob.Get());
    pipelineStateData.desc.PS = CD3DX12_SHADER_BYTECODE(m_pPixelShaderResource->pShaderBlob.Get());

    m_pPipelineState = pipelineStateData.pPSOLibrary->Assign(
        pDevice,
        (std::wstring(shaderData.vertexShaderFilepath)
            + std::wstring(shaderData.pixelShaderFilepath)
            ).c_str(),
        &pipelineStateData.desc
    );
}

void RenderObject::Render(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT rect,
    D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs,
    size_t rtvsCount,
    D3D12_CPU_DESCRIPTOR_HANDLE* pDSV,
    std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT& rootParameterIndex)> outerRootParametersSetter
) const {
    assert(pCommandListDirect->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);

    SetPsoRs(pCommandListDirect);

    pCommandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    pCommandListDirect->RSSetViewports(1, &viewport);
    pCommandListDirect->RSSetScissorRects(1, &rect);

    pCommandListDirect->OMSetRenderTargets(static_cast<UINT>(rtvsCount), pRTVs, FALSE, pDSV);

    RenderJob(pCommandListDirect);

    UINT rootParameterIndex{};
    outerRootParametersSetter(pCommandListDirect, rootParameterIndex);
    InnerRootParametersSetter(pCommandListDirect, rootParameterIndex);

    pCommandListDirect->DrawIndexedInstanced(
        GetIndexCountPerInstance(),
        GetInstanceCount(),
        0, 0, 0
    );
}

void RenderObject::SetPsoRs(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const
{
    assert(pCommandListDirect->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);

    pCommandListDirect->SetPipelineState(m_pPipelineState.Get());
    pCommandListDirect->SetGraphicsRootSignature(m_pRootSignatureResource->pRootSignature.Get());
} 

void RenderObject::RenderJob(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const {}

void RenderObject::InnerRootParametersSetter(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, UINT& rootParamId) const {}

UINT RenderObject::GetIndexCountPerInstance() const {
    return 0;
}

UINT RenderObject::GetInstanceCount() const {
    return 0;
}
