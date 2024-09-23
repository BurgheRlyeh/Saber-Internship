#pragma once

#include "Headers.h"

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
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas{};
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob{};
        std::wstring rootSignatureFilename{};

        RootSignatureData(
            std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
            Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob,
            std::wstring rootSignatureFilename
        ) : pRootSignatureAtlas(pRootSignatureAtlas),
            pRootSignatureBlob(pRootSignatureBlob),
            rootSignatureFilename(rootSignatureFilename)
        {}
    };
    struct ComputeShaderData {
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas{};
        LPCWSTR computeShaderFilepath{};

        ComputeShaderData(
            std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
            LPCWSTR computeShaderFilepath
        ) : pShaderAtlas(pShaderAtlas),
            computeShaderFilepath(computeShaderFilepath)
        {}
    };
    void InitMaterial(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        const RootSignatureData& rootSignatureData,
        const ComputeShaderData& shaderData,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        m_pRootSignatureResource = rootSignatureData.pRootSignatureAtlas->Assign(
            rootSignatureData.rootSignatureFilename,
            pDevice,
            rootSignatureData.pRootSignatureBlob
        );

        m_pComputeShaderResource = shaderData.pShaderAtlas->Assign(shaderData.computeShaderFilepath);

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{
            .pRootSignature{ m_pRootSignatureResource->pRootSignature.Get() },
            .CS{ CD3DX12_SHADER_BYTECODE(m_pComputeShaderResource->pShaderBlob.Get()) }
        };

        m_pPipelineState = pPSOLibrary->Assign(
            pDevice,
            shaderData.computeShaderFilepath,
            &desc
        );
    }

    virtual void Dispatch(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute,
        UINT threadGroupsCountX,
        UINT threadGroupsCountY,
        UINT threadGroupsCountZ,
        std::function<void(
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
            UINT& rootParamId
        )> outerRootParametersSetter = [](
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
            UINT& rootParamId
            ) {}
    ) {
        pCommandListCompute->SetPipelineState(m_pPipelineState.Get());
        pCommandListCompute->SetComputeRootSignature(m_pRootSignatureResource->pRootSignature.Get());

        DispatchJob(pCommandListCompute);

        UINT rootParamId{};
        outerRootParametersSetter(pCommandListCompute, rootParamId);
        InnerRootParametersSetter(pCommandListCompute, rootParamId);

        pCommandListCompute->Dispatch(threadGroupsCountX, threadGroupsCountY, threadGroupsCountZ);
    }
    
protected:
    virtual void DispatchJob(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const {}
    virtual void InnerRootParametersSetter(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        UINT& rootParamId
    ) const {}
};

class DeferredShading : ComputeObject {
public:
    static std::shared_ptr<ComputeObject> CreateDefferedShadingComputeObject(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
        pComputeObj->InitMaterial(
            pDevice,
            RootSignatureData(
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"DeferredShadingRootSignature"
            ),
            ComputeShaderData(
                pShaderAtlas,
                L"DeferredShadingComputeShader.cso"
            ),
            pPSOLibrary
        );

        return pComputeObj;
    }

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
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
        rangeCbvsMaterials[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, -1, 2);
        rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeCbvsMaterials), rangeCbvsMaterials);
         
        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvsMaterial[1]{};
        rangeSrvsMaterial[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 3);
        rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeSrvsMaterial), rangeSrvsMaterial);

        D3D12_STATIC_SAMPLER_DESC sampler{
            .Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT },
            .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
            .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
            .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
            .MipLODBias{},
            .MaxAnisotropy{},
            .ComparisonFunc{ D3D12_COMPARISON_FUNC_NEVER },
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
        if (FAILED(pDevice->CheckFeatureSupport(
            D3D12_FEATURE_ROOT_SIGNATURE,
            &featureData,
            sizeof(featureData)
        ))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        HRESULT hr{ D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
            &rootSignatureBlob,
            &errorBlob
        ) };
        if (FAILED(hr) && errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        ThrowIfFailed(hr);

        return rootSignatureBlob;
    }
};
