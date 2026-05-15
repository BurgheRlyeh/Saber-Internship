/**
 * @file ComputeObject.h
 * @brief Wraps a compute pipeline (root signature + shader + PSO) into a
 *        single dispatchable object.
 */
#pragma once

#include "Headers.h"

#include <functional>

class CommandList;
class DeviceContext;
class PSOLibrary;
class RootSignatureResource;
class ShaderResource;

/**
 * @brief Encapsulates a D3D12 compute pipeline state together with its root signature.
 *
 * Call @ref InitMaterial once to build the pipeline, then call @ref Dispatch
 * each frame to execute the compute shader.  Subclasses may override
 * @ref InnerRootParametersSetter to bind additional root parameters before
 * the outer caller binds its own.
 */
class ComputeObject {
protected:
    std::shared_ptr<RootSignatureResource> m_pRootSignatureResource{}; /**< @brief Compiled root signature. */
    std::shared_ptr<ShaderResource> m_pComputeShaderResource{};        /**< @brief Compiled compute shader blob. */
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState{};    /**< @brief D3D12 compute PSO. */

public:
    /** @brief Data needed to load or create the root signature. */
    struct RootSignatureData {
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob{}; /**< @brief Pre-serialised root-signature blob. */
        std::wstring rootSignatureFilename{};                  /**< @brief Cache key used in the atlas. */
    };

    /** @brief Data needed to load the compute shader. */
    struct ComputeShaderData {
        std::wstring computeShaderFilepath{}; /**< @brief Path to the compiled @c .cso file. */
    };

    /**
     * @brief Creates the root signature and pipeline state from the supplied descriptors.
     * @param pDeviceContext   Device context providing the D3D12 device and PSO library.
     * @param rootSignatureData Root-signature blob and cache filename.
     * @param shaderData        Path to the compiled compute shader.
     */
    void InitMaterial(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const RootSignatureData& rootSignatureData,
        const ComputeShaderData& shaderData
    );

    /**
     * @brief Sets the compute pipeline, binds root parameters, and dispatches thread groups.
     *
     * The dispatch sequence is:
     *  1. Set PSO and root signature.
     *  2. Call @ref InnerRootParametersSetter (subclass hook).
     *  3. Call @p outerRootParametersSetter (caller-supplied bindings).
     *  4. Dispatch @p threadGroupsCount thread groups.
     *
     * @param pCommandList            Command list to record into.
     * @param threadGroupsCount       Number of thread groups in X, Y, Z.
     * @param outerRootParametersSetter Callback that binds additional root parameters
     *                                  after the inner ones are set.
     */
    virtual void Dispatch(
        std::shared_ptr<CommandList> pCommandList,
        DirectX::XMUINT3 threadGroupsCount,
        std::function<void(
            std::shared_ptr<CommandList> pCommandList,
            UINT& rootParamId
        )> outerRootParametersSetter = [](auto, auto) {}
    ) const;

protected:
    /**
     * @brief Sets the PSO and root signature on the command list.
     * @param pCommandList Command list to configure.
     */
    virtual void DispatchJob(std::shared_ptr<CommandList> pCommandList) const;

    /**
     * @brief Subclass hook to bind class-specific root parameters before the outer callback.
     * @param pCommandList  Command list to record root-parameter commands into.
     * @param rootParamId   Current root-parameter index; increment for each bound slot.
     */
    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const;
};
