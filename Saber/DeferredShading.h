/**
 * @file DeferredShading.h
 * @brief Factory for the deferred-shading compute pass that resolves the G-buffer.
 */
#pragma once

#include "Headers.h"

#include "ComputeObject.h"

class DeviceContext;

/**
 * @brief Specialised @ref ComputeObject that implements the deferred-shading lighting pass.
 *
 * Reads from the G-buffer SRVs and outputs shaded colour to a UAV.
 * Instantiated via the static factory @ref CreateDefferedShadingComputeObject.
 */
class DeferredShading : public ComputeObject {
public:
    /**
     * @brief Creates and initialises the deferred-shading compute object.
     * @param pDeviceContext Device context used to build the PSO and root signature.
     * @return Shared pointer to the initialised compute object.
     */
    static std::shared_ptr<ComputeObject> CreateDefferedShadingComputeObject(
        std::shared_ptr<DeviceContext> pDeviceContext
    );

private:
    /**
     * @brief Builds and serialises the root signature blob for the deferred-shading shader.
     * @return Serialised root-signature blob.
     */
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob();
};
