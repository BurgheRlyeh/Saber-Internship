/**
 * @file Resource.h
 * @brief Lightweight GPU resource descriptors for shaders and root signatures.
 *
 * @ref ShaderResource loads a compiled shader blob from a @c .cso file.
 * @ref RootSignatureResource deserialises a root-signature blob and creates
 * the @c ID3D12RootSignature object on the device.
 */
#pragma once

#include "Headers.h"

#include "Device.h"

/**
 * @brief Holds a compiled shader blob loaded from a @c .cso file.
 */
struct ShaderResource {
    Microsoft::WRL::ComPtr<ID3DBlob> pShaderBlob{}; /**< @brief The compiled shader byte-code. */

    /**
     * @brief Reads and stores the compiled shader from disk.
     * @param filename Path to the compiled shader object (@c .cso) file.
     */
    ShaderResource(const std::wstring& filename) {
        ThrowIfFailed(D3DReadFileToBlob(filename.c_str(), &pShaderBlob));
    }
};

/**
 * @brief Creates and holds a @c ID3D12RootSignature from a serialised blob.
 */
struct RootSignatureResource {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature{}; /**< @brief The created root signature object. */

    /**
     * @brief Deserialises the blob and creates the root signature on the device.
     * @param filename            Debug name applied to the root-signature object.
     * @param pDevice             Device on which to create the root signature.
     * @param pRootSignatureBlob  Serialised root-signature blob.
     */
    RootSignatureResource(
        const std::wstring& filename,
        std::shared_ptr<Device> pDevice,
        Microsoft::WRL::ComPtr<ID3DBlob> pRootSignatureBlob
    ) {
        ThrowIfFailed(pDevice->GetD3D12Device()->CreateRootSignature(
            0,
            pRootSignatureBlob->GetBufferPointer(),
            pRootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&pRootSignature)
        ));
        pRootSignature->SetName(filename.c_str());
    }
};
