#pragma once

#include "Headers.h"

#include "Device.h"
#include "DeviceContext.h"
#include "Atlas.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"

class ComputeObject {
protected:
    std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{};
    std::shared_ptr<ShaderResource> m_pComputeShaderResource{};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState{};

public:
    struct RootSignatureData {
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob{};
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
        )> outerRootParametersSetter = [](
            std::shared_ptr<CommandList> pCommandList,
            UINT& rootParamId
        ) {}
    );
    
protected:
    virtual void DispatchJob(std::shared_ptr<CommandList> pCommandList) const {}
    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const {}
};

class DeferredShading : ComputeObject {
public:
    static std::shared_ptr<ComputeObject> CreateDefferedShadingComputeObject(
        std::shared_ptr<DeviceContext> pDeviceContext
    ) {
        std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
        pComputeObj->InitMaterial(
            pDeviceContext,
            RootSignatureData(
                CreateRootSignatureBlob(pDeviceContext->GetDevice()),
                L"DeferredShadingRootSignature"
            ),
            ComputeShaderData(
                L"DeferredShadingComputeShader.cso"
            )
        );

        return pComputeObj;
    }

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        std::shared_ptr<Device> pDevice
    ) {
        size_t rpId{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[7]{};
        rootParameters[rpId++].InitAsConstantBufferView(0);
        rootParameters[rpId++].InitAsConstantBufferView(1); 

        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvsGbuffer[1]{};
        rangeSrvsGbuffer[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
        rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeSrvsGbuffer), rangeSrvsGbuffer);

        CD3DX12_DESCRIPTOR_RANGE1 rangeUavsGbuffer[1]{};
        rangeUavsGbuffer[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeUavsGbuffer), rangeUavsGbuffer);

        CD3DX12_DESCRIPTOR_RANGE1 rangeDepthBuffer[1]{};
        rangeDepthBuffer[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeDepthBuffer), rangeDepthBuffer);

        CD3DX12_DESCRIPTOR_RANGE1 rangeCbvsMaterials[1]{};
        rangeCbvsMaterials[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
        rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeCbvsMaterials), rangeCbvsMaterials);
         
        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvsMaterial[1]{};
        rangeSrvsMaterial[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
        rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeSrvsMaterial), rangeSrvsMaterial);

        D3D12_STATIC_SAMPLER_DESC sampler{
            .Filter{ D3D12_FILTER_ANISOTROPIC },
            .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
            .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
            .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
            .MipLODBias{},
            .MaxAnisotropy{ 16 },
            .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
            .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
            .MinLOD{},
            .MaxLOD{ D3D12_FLOAT32_MAX },
            .ShaderRegister{},
            .RegisterSpace{},
            .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
        };

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler);

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->GetD3D12Device()->CheckFeatureSupport(
            D3D12_FEATURE_ROOT_SIGNATURE,
            &featureData,
            sizeof(featureData)
        ))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
            &rootSignatureBlob,
            &errorBlob
        ));

        return rootSignatureBlob;
    }
};
