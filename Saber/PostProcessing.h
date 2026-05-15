/**
 * @file PostProcessing.h
 * @brief Fullscreen copy post-processing pass.
 *
 * @ref CopyPostProcessing is a @ref FullscreenDrawPass that blits a source
 * texture to the current render target using a trivial vertex + pixel shader pair.
 * It is used as the final resolve step that copies the deferred-shading result
 * into the swap-chain back buffer.
 */
#pragma once

#include "Headers.h"

#include "RenderObject.h"

class DeviceContext;

/**
 * @brief Fullscreen blit pass; copies a bound SRV to the active render target.
 *
 * Inherits @ref FullscreenDrawPass which issues a @c DrawInstanced(3, 1, 0, 0)
 * call covering the whole viewport without a vertex buffer.
 */
class CopyPostProcessing : public FullscreenDrawPass {
    using FullscreenDrawPass::FullscreenDrawPass;

public:
    /**
     * @brief Constructs the pass, initialising shaders, root signature, and PSO.
     * @param pDeviceContext Device context providing the PSO library and descriptor heaps.
     */
    CopyPostProcessing(
        std::shared_ptr<DeviceContext> pDeviceContext
    );

private:
    /** @brief Creates the serialised root signature blob for the copy pass. */
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob();
    /** @brief Creates the graphics pipeline state descriptor for the copy pass. */
    static D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc();
};
