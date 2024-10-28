#pragma once

#include <GLTFSDK/GLTF.h>

#include "Headers.h"

#include <filesystem>
#include <initializer_list>

#include "Atlas.h"
#include "CommandQueue.h"
#include "ConstantBuffer.h"
#include "Mesh.h"
#include "PSOLibrary.h"
#include "Resources.h"
#include "Vertices.h"

class RenderObject {
protected:
    std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{};
    std::shared_ptr<ShaderResource> m_pVertexShaderResource{};
    std::shared_ptr<ShaderResource> m_pPixelShaderResource{};
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
    struct ShaderData {
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas{};
        LPCWSTR vertexShaderFilepath{};
        LPCWSTR pixelShaderFilepath{};

        ShaderData(
            std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
            LPCWSTR vertexShaderFilepath,
            LPCWSTR pixelShaderFilepath
        ) : pShaderAtlas(pShaderAtlas),
            vertexShaderFilepath(vertexShaderFilepath),
            pixelShaderFilepath(pixelShaderFilepath)
        {}
    };
    struct PipelineStateData {
        std::shared_ptr<PSOLibrary> pPSOLibrary{};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        
        PipelineStateData(
            std::shared_ptr<PSOLibrary> pPSOLibrary,
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc
        ) : pPSOLibrary(pPSOLibrary),
            desc(desc)
        {}
    };
    void InitMaterial(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        const RootSignatureData& rootSignatureData,
        const ShaderData& shaderData,
        PipelineStateData& pipelineStateData
    );

    virtual void Render(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT rect,
        D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs,
        size_t rtvsCount,
        D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilView,
        std::function<
            void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT& rootParameterIndex)
        > outerRootParametersSetter = [](
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
            UINT& rootParamId
        ) {}
    ) const;

    void SetPsoRs(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const;

    //virtual void FillIndirectDrawParameters() const;

protected:
    virtual void RenderJob(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect) const;
    virtual void InnerRootParametersSetter(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        UINT& rootParamId
    ) const;

    virtual UINT GetIndexCountPerInstance() const;
    virtual UINT GetInstanceCount() const;
};
