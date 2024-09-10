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
        CD3DX12_DESCRIPTOR_RANGE1 rangeDescs[2]{};
        rangeDescs[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
        rangeDescs[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[3]{};
        rootParameters[0].InitAsConstantBufferView(0);
        rootParameters[1].InitAsConstantBufferView(1);
        rootParameters[2].InitAsDescriptorTable(_countof(rangeDescs), rangeDescs);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters);

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
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
