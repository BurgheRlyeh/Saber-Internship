#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

#include "Atlas.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"

class PostProcessing : protected RenderObject {
public:
    using RenderObject::InitMaterial;
    
    virtual void Render(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT rect,
        D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs,
        size_t rtvsCount,
        std::function<
            void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT& rootParameterIndex)
        > outerRootParametersSetter = [](
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
            UINT& rootParamId
        ) {}
    );

protected:
    UINT GetIndexCountPerInstance() const override;
    UINT GetInstanceCount() const override;
};

class CopyPostProcessing : public PostProcessing {
public:
    static std::shared_ptr<PostProcessing> Create(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<Mesh>> pMeshAtlas,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        std::shared_ptr<PostProcessing> pp{ std::make_shared<PostProcessing>() };
        pp->InitMaterial(
            pDevice,
            RootSignatureData{
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"CopyPostProcessingRootSignature"
            },
            ShaderData{
                pShaderAtlas,
                L"CopyPostProcessingVS.cso",
                L"CopyPostProcessingPS.cso"
            },
            PipelineStateData{
                pPSOLibrary,
                CreatePipelineStateDesc()
            }
        );

        return pp;
    }

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    ) {
        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        };

        CD3DX12_DESCRIPTOR_RANGE1 rangeDescs[1]{};
        rangeDescs[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER1 rootParameters[1]{};
        rootParameters[0].InitAsDescriptorTable(_countof(rangeDescs), rangeDescs, D3D12_SHADER_VISIBILITY_PIXEL);

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
            .ShaderVisibility{ D3D12_SHADER_VISIBILITY_PIXEL }
        };

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

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

    static D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc() {
        CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };
        rasterizerDesc.FrontCounterClockwise = true;

        D3D12_RT_FORMAT_ARRAY rtvFormats{};
        rtvFormats.NumRenderTargets = 1;
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.RasterizerState = rasterizerDesc;
        pipelineStateStream.RTVFormats = rtvFormats;

        return pipelineStateStream.GraphicsDescV0();
    }
};
