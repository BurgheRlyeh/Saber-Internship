/**
 * @file IndirectUpdater.h
 * @brief GPU-side compute pass that patches indirect command buffers in-place.
 *
 * Provides static factories for compute objects that update specific
 * @ref IndirectCommand layouts via compute shaders.
 */
#pragma once

#include "ComputeObject.h"
#include "DeviceContext.h"

/**
 * @brief Factory class for compute objects that update indirect draw command buffers on the GPU.
 *
 * Each static factory method builds and returns a @ref ComputeObject configured
 * with the appropriate compute shader and root signature for a given
 * indirect-command layout.
 */
class IndirectUpdater : ComputeObject {
public:
    /**
     * @brief Creates an updater for @c ConstMesh4IndirectCommand buffers.
     * @param pDeviceContext Device context for pipeline creation.
     * @return Initialised compute object ready for dispatch.
     */
    static std::shared_ptr<ComputeObject> CreateConstMesh4Updater(
        std::shared_ptr<DeviceContext> pDeviceContext
    ) {
        return Create(
            pDeviceContext,
            L"IndirectUpdaterConstMesh4.cso"
        );
    }

private:
    /**
     * @brief Internal helper that builds a compute object from the given shader file.
     * @param pDeviceContext Device context for pipeline creation.
     * @param filename       Path to the compiled compute shader (@c .cso).
     * @return Initialised compute object.
     */
    static std::shared_ptr<ComputeObject> Create(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::wstring& filename
    ) {
        std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
        pComputeObj->InitMaterial(
            pDeviceContext,
            RootSignatureData{
                CreateRootSignatureBlob(),
                L"IndirectUpdaterRootSignature"
            },
            ComputeShaderData{ filename }
        );

        return pComputeObj;
    }

    /**
     * @brief Builds the root signature blob for indirect-updater compute shaders.
     *
     * Root parameters (in order):
     *  0. 4 × 32-bit constants (update count + padding)
     *  1. SRV b0 — buffer of element indices to update
     *  2. SRV b1 — buffer of new element values
     *  3. UAV u0 — target indirect command buffer
     *
     * @return Serialised root-signature blob.
     */
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob() {
        size_t rpId{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[4]{};
        rootParameters[rpId++].InitAsConstants(4, 0);
        rootParameters[rpId++].InitAsShaderResourceView(0);
        rootParameters[rpId++].InitAsShaderResourceView(1);
        rootParameters[rpId++].InitAsUnorderedAccessView(0);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters);

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        HRESULT hr{ D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            D3D_ROOT_SIGNATURE_VERSION_1_1,
            &rootSignatureBlob,
            &errorBlob
        ) };
        if (FAILED(hr) && errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        ThrowIfFailed(hr);

        return rootSignatureBlob;
    }
};
