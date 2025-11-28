#include "ComputeObject.h"

#include "CommandList.h"
#include "DeviceContext.h"
#include "PSOLibrary.h"
#include "Resources.h"

void ComputeObject::InitMaterial(
    std::shared_ptr<DeviceContext> pDeviceContext,
    const RootSignatureData& rootSignatureData,
    const ComputeShaderData& shaderData
) {
    m_pRootSignatureResource = pDeviceContext->GetRootSignatureAtlas()->Assign(
        rootSignatureData.rootSignatureFilename,
        pDeviceContext->GetDevice(),
        rootSignatureData.pRootSignatureBlob
    );

    m_pComputeShaderResource = pDeviceContext->GetShaderAtlas()->Assign(shaderData.computeShaderFilepath);

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{
        .pRootSignature{ m_pRootSignatureResource->pRootSignature.Get() },
        .CS{ CD3DX12_SHADER_BYTECODE(m_pComputeShaderResource->pShaderBlob.Get()) }
    };

    m_pPipelineState = pDeviceContext->GetPSOLibrary()->Assign(
        pDeviceContext->GetDevice(),
        shaderData.computeShaderFilepath,
        &desc
    );
}

void ComputeObject::Dispatch(
    std::shared_ptr<CommandList> pCommandList,
    DirectX::XMUINT3 threadGroupsCount,
    std::function<void(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    )> outerRootParametersSetter
) const {
    pCommandList->GetD3D12CommandList()->SetPipelineState(m_pPipelineState.Get());
    pCommandList->GetD3D12CommandList()->SetComputeRootSignature(m_pRootSignatureResource->pRootSignature.Get());

    DispatchJob(pCommandList);

    UINT rootParamId{};
    outerRootParametersSetter(pCommandList, rootParamId);
    InnerRootParametersSetter(pCommandList, rootParamId);

    pCommandList->GetD3D12CommandList()->Dispatch(threadGroupsCount.x, threadGroupsCount.y, threadGroupsCount.z);
}

void ComputeObject::DispatchJob(
    std::shared_ptr<CommandList> pCommandList
) const {}

void ComputeObject::InnerRootParametersSetter(
    std::shared_ptr<CommandList> pCommandList,
    UINT& rootParamId
) const {}
