#pragma once

#include "Headers.h"

#include "IndirectCommand.h"

class CommandList;
class DeviceContext;
class RootSignatureResource;
class ShaderResource;

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
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob{};
        std::wstring rootSignatureFilename{};
    };
    struct ShaderData {
        std::wstring vertexShaderFilepath{};
        std::wstring pixelShaderFilepath{};
    };
    struct PipelineStateData {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    };
    void InitMaterial(
        std::shared_ptr<DeviceContext> pDeviceContext,
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
        std::shared_ptr<CommandList> pCommandList
    ) const;

    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const;

    virtual void DrawCall(
        std::shared_ptr<CommandList> pCommandList
    ) const = 0;
};

class FullscreenDrawPass : public RenderObject {
protected:
    virtual void DrawCall(
        std::shared_ptr<CommandList> pCommandList
    ) const override;
};
