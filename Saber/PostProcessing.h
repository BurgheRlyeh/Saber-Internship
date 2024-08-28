#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

#include "Atlas.h"
#include "PSOLibrary.h"
#include "Resources.h"

class PostProcessing {
	std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{};
    std::shared_ptr<ShaderResource> m_pVertexShaderResource{};
    std::shared_ptr<ShaderResource> m_pPixelShaderResource{};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState{};

public:
    void InitMaterial(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        const LPCWSTR& vertexShaderFilepath,
        const LPCWSTR& pixelShaderFilepath,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob,
        const std::wstring& rootSignatureFilename,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        m_pVertexShaderResource = pShaderAtlas->Assign(vertexShaderFilepath);
        m_pPixelShaderResource = pShaderAtlas->Assign(pixelShaderFilepath);

        RootSignatureResource::RootSignatureResourceData rootSignatureResData{
            .pDevice{ pDevice },
            .pRootSignatureBlob{ pRootSignatureBlob }
        };
        m_pRootSignatureResource = pRootSignatureAtlas->Assign(rootSignatureFilename, rootSignatureResData);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
            CreatePipelineStateDesc(m_pRootSignatureResource, m_pVertexShaderResource, m_pPixelShaderResource)
        };

        m_pPipelineState = pPSOLibrary->Assign(
            pDevice,
            (std::wstring(vertexShaderFilepath) + std::wstring(pixelShaderFilepath)).c_str(),
            &desc
        );
    }

    void Render(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT rect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pTexDescHeap
    ) const {
        pCommandListDirect->SetPipelineState(m_pPipelineState.Get());

        pCommandListDirect->SetGraphicsRootSignature(m_pRootSignatureResource->pRootSignature.Get());

        pCommandListDirect->SetDescriptorHeaps(1, pTexDescHeap.GetAddressOf());
        pCommandListDirect->SetGraphicsRootDescriptorTable(0, pTexDescHeap->GetGPUDescriptorHandleForHeapStart());

        pCommandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandListDirect->RSSetViewports(1, &viewport);
        pCommandListDirect->RSSetScissorRects(1, &rect);

        pCommandListDirect->OMSetRenderTargets(1, &renderTargetView, FALSE, nullptr);

        pCommandListDirect->DrawInstanced(3, 1, 0, 0);
    }

private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc(
        std::shared_ptr<RootSignatureResource> pRootSignatureResource,
        std::shared_ptr<ShaderResource> pVertexShaderResource,
        std::shared_ptr<ShaderResource> pPixelShaderResource
    ) {
        D3D12_RT_FORMAT_ARRAY rtvFormats{ .NumRenderTargets{ 1 } };
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
        pipelineStateStream.pRootSignature = pRootSignatureResource->pRootSignature.Get();
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderResource->pShaderBlob.Get());
        pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderResource->pShaderBlob.Get());
        pipelineStateStream.RTVFormats = rtvFormats;

        return pipelineStateStream.GraphicsDescV0();
    }
};

class SimplePostProcessing : PostProcessing {
public:


private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    ) {
        CD3DX12_DESCRIPTOR_RANGE1 rangeDescs[1]{};
        rangeDescs[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

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
        //CD3DX12_STATIC_SAMPLER_DESC sampler{};
        //sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        //sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        //sampler.MaxAnisotropy = 0;
        ////sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        ////sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        //sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
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
};