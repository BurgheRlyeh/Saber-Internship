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
    std::shared_ptr<CommandList> pCommandListDirect,
    UINT rootParameterIndex
) const {
    RenderJob(pCommandListDirect);
    InnerRootParametersSetter(pCommandListDirect, rootParameterIndex);
    
    DrawCall(pCommandListDirect);
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> RenderObject::GetPipelineState() const {
    return m_pPipelineState;
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> RenderObject::GetRootSignature() const {
    return m_pRootSignatureResource->pRootSignature;
}

void RenderObject::SetPipelineStateAndRootSignature(std::shared_ptr<CommandList> pCommandList) const {
    auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };
    pD3D12CommandList->SetPipelineState(m_pPipelineState.Get());
    pD3D12CommandList->SetGraphicsRootSignature(m_pRootSignatureResource->pRootSignature.Get());
}

void RenderObject::RenderJob(std::shared_ptr<CommandList> pCommandListDirect) const {}

void RenderObject::InnerRootParametersSetter(std::shared_ptr<CommandList> pCommandListDirect, UINT& rootParamId) const {}

