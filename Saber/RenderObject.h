#pragma once

#include <GLTFSDK/GLTF.h>

#include "Headers.h"

#include <filesystem>
#include <initializer_list>

#include "Atlas.h"
#include "CommandQueue.h"
#include "ConstantBuffer.h"
#include "IndirectCommand.h"
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

    RenderObject() = default;
    RenderObject(const RenderObject&) = default;
    RenderObject(RenderObject&&) = default;

public:
    struct RootSignatureData {
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas{};
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob{};
        std::wstring rootSignatureFilename{};
    };
    struct ShaderData {
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas{};
        LPCWSTR vertexShaderFilepath{};
        LPCWSTR pixelShaderFilepath{};
    };
    struct PipelineStateData {
        std::shared_ptr<PSOLibrary> pPSOLibrary{};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    };
    void InitMaterial(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        const RootSignatureData& rootSignatureData,
        const ShaderData& shaderData,
        PipelineStateData& pipelineStateData
    );

    virtual void FillIndirectCommand(CbMeshIndirectCommand& indirectCommand) {}
    virtual void FillIndirectCommand(CbMesh4IndirectCommand& indirectCommand) {}
    virtual void FillIndirectCommand(ConstMesh4IndirectCommand& indirectCommand) {}
    virtual void FillIndirectCommand(CbConstMesh4IndirectCommand& indirectCommand) {}

	virtual void Render(
        std::shared_ptr<CommandList> pCommandListDirect,
		UINT rootParameterIndex
    ) const;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const;

	void SetPipelineStateAndRootSignature(
        std::shared_ptr<CommandList> pCommandList
	) const;

protected:
    virtual void RenderJob(
        std::shared_ptr<CommandList> pCommandListDirect
    ) const;

    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const;

    virtual void DrawCall(
        std::shared_ptr<CommandList> pCommandList
    ) const = 0;
};
