#pragma once

#include "Headers.h"

#include <functional>

class CommandList;
class DeviceContext;
class PSOLibrary;
class RootSignatureResource;
class ShaderResource;

class ComputeObject {
protected:
    std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{};
    std::shared_ptr<ShaderResource> m_pComputeShaderResource{};
    Microsoft::WRL::ComPtr<D3D12PipelineState> m_pPipelineState{};

public:
    struct RootSignatureData {
        Microsoft::WRL::ComPtr<D3DBlob> pRootSignatureBlob{};
        std::wstring rootSignatureFilename{};
    };
    struct ComputeShaderData {
        std::wstring computeShaderFilepath{};
    };
    void InitMaterial(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const RootSignatureData& rootSignatureData,
        const ComputeShaderData& shaderData
    );

    virtual void Dispatch(
        std::shared_ptr<CommandList> pCommandList,
        DirectX::XMUINT3 threadGroupsCount,
        std::function<void(
            std::shared_ptr<CommandList> pCommandList,
            UINT& rootParamId
        )> outerRootParametersSetter = [](auto, auto) {}
    ) const;
    
protected:
    virtual void DispatchJob(std::shared_ptr<CommandList> pCommandList) const;
    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const;
};
