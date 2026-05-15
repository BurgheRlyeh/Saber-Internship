/**
 * @file RenderObject.h
 * @brief Base class for all render objects and a fullscreen draw-pass variant.
 *
 * @ref RenderObject holds the D3D12 root signature, VS/PS shader blobs, and
 * the compiled PSO.  Subclasses implement @ref DrawCall and optionally override
 * @ref RenderJob and @ref InnerRootParametersSetter to add per-draw state.
 *
 * @ref FullscreenDrawPass derives from @ref RenderObject and overrides
 * @ref DrawCall to issue a three-vertex fullscreen triangle without a vertex
 * buffer — suitable for post-processing and deferred-shading resolve passes.
 */
#pragma once

#include "Headers.h"

#include "IndirectCommand.h"

class CommandList;
class DeviceContext;
class RootSignatureResource;
class ShaderResource;

/**
 * @brief Abstract base class for objects that can be drawn with a Direct command list.
 *
 * @ref InitMaterial compiles (or retrieves from the PSO library) the pipeline state.
 * @ref Render sequences @ref RenderJob → @ref InnerRootParametersSetter → @ref DrawCall.
 */
class RenderObject {
protected:
    std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{}; /**< @brief Cached root signature. */
    std::shared_ptr<ShaderResource> m_pVertexShaderResource{};         /**< @brief Cached vertex shader blob. */
    std::shared_ptr<ShaderResource> m_pPixelShaderResource{};          /**< @brief Cached pixel shader blob. */
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState{};    /**< @brief Compiled pipeline state. */

    RenderObject() = default;
    RenderObject(const RenderObject&) = default;
    RenderObject(RenderObject&&) = default;

public:
    /** @brief Aggregates root-signature serialisation data for @ref InitMaterial. */
    struct RootSignatureData {
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob{}; /**< @brief Serialised root signature. */
        std::wstring rootSignatureFilename{};                   /**< @brief Cache key for the root signature atlas. */
    };

    /** @brief Shader file paths for @ref InitMaterial. */
    struct ShaderData {
        std::wstring vertexShaderFilepath{}; /**< @brief Path to the compiled vertex shader (.cso). */
        std::wstring pixelShaderFilepath{};  /**< @brief Path to the compiled pixel shader (.cso). */
    };

    /** @brief Graphics PSO descriptor for @ref InitMaterial. */
    struct PipelineStateData {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{}; /**< @brief D3D12 pipeline state descriptor. */
    };

    /**
     * @brief Compiles (or retrieves from cache) the root signature and PSO.
     * @param pDeviceContext    Device context providing the PSO library and shader atlas.
     * @param rootSignatureData Root signature blob and cache key.
     * @param shaderData        VS and PS file paths.
     * @param pipelineStateData Graphics PSO descriptor; VS/PS and root signature are injected.
     */
    void InitMaterial(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const RootSignatureData& rootSignatureData,
        const ShaderData& shaderData,
        PipelineStateData& pipelineStateData
    );

    /** @brief No-op override for @c CbMeshIndirectCommand; subclasses may override. */
    virtual void FillIndirectCommand(CbMeshIndirectCommand& indirectCommand) {}
    /** @brief No-op override for @c CbMesh4IndirectCommand; subclasses may override. */
    virtual void FillIndirectCommand(CbMesh4IndirectCommand& indirectCommand) {}
    /** @brief No-op override for @c ConstMesh4IndirectCommand; subclasses may override. */
    virtual void FillIndirectCommand(ConstMesh4IndirectCommand& indirectCommand) {}
    /** @brief No-op override for @c CbConstMesh4IndirectCommand; subclasses may override. */
    virtual void FillIndirectCommand(CbConstMesh4IndirectCommand& indirectCommand) {}

    /**
     * @brief Records the draw call for this object.
     *
     * Calls @ref RenderJob → @ref InnerRootParametersSetter → @ref DrawCall in sequence.
     *
     * @param pCommandListDirect Direct command list.
     * @param rootParameterIndex First root-parameter index reserved for per-object data.
     */
    virtual void Render(
        std::shared_ptr<CommandList> pCommandListDirect,
        UINT rootParameterIndex
    ) const;

    /** @brief Returns the compiled @c ID3D12PipelineState. */
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const;

    /** @brief Returns the @c ID3D12RootSignature associated with this object. */
    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const;

    /**
     * @brief Binds the PSO and root signature on the command list.
     * @param pCommandList Command list to bind into.
     */
    void SetPipelineStateAndRootSignature(
        std::shared_ptr<CommandList> pCommandList
    ) const;

protected:
    /**
     * @brief Per-draw setup hook (e.g. bind vertex/index buffers).
     *
     * Called before @ref InnerRootParametersSetter.  Default implementation is a no-op.
     *
     * @param pCommandList Command list.
     */
    virtual void RenderJob(
        std::shared_ptr<CommandList> pCommandList
    ) const;

    /**
     * @brief Binds per-object root parameters.
     *
     * Called after @ref RenderJob and before @ref DrawCall.
     * Default implementation is a no-op.
     *
     * @param pCommandList Command list.
     * @param rootParamId  In/out: first available root parameter index; increment for each bound parameter.
     */
    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const;

    /**
     * @brief Issues the actual draw command (e.g. @c DrawIndexedInstanced).
     *
     * Pure virtual; every concrete render object must provide an implementation.
     *
     * @param pCommandList Command list.
     */
    virtual void DrawCall(
        std::shared_ptr<CommandList> pCommandList
    ) const = 0;
};

/**
 * @brief Renders a fullscreen triangle without a vertex buffer.
 *
 * Overrides @ref DrawCall with @c DrawInstanced(3, 1, 0, 0).  The vertex
 * shader is expected to generate clip-space positions from @c SV_VertexID.
 */
class FullscreenDrawPass : public RenderObject {
protected:
    /**
     * @brief Issues @c DrawInstanced(3, 1, 0, 0) for a fullscreen triangle.
     * @param pCommandList Command list.
     */
    virtual void DrawCall(
        std::shared_ptr<CommandList> pCommandList
    ) const override;
};
