#pragma once

#include "Atlas.h"
#include "ComputeObject.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"

class IndirectUpdater : ComputeObject {
public:
    static std::shared_ptr<ComputeObject> CreateConstMesh4Updater(
        std::shared_ptr<DeviceContext> pDeviceContext
    ) {
        return Create(
            pDeviceContext,
            L"IndirectUpdaterConstMesh4.cso"
        );
    }

private:
    static std::shared_ptr<ComputeObject> Create(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::wstring& filename
    ) {
        std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
        pComputeObj->InitMaterial(
            pDeviceContext,
            RootSignatureData{
                CreateRootSignatureBlob(pDeviceContext->GetDevice()),
                L"IndirectUpdaterRootSignature"
            },
            ComputeShaderData{ filename }
        );

        return pComputeObj;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        std::shared_ptr<Device> pDevice
    ) {
        size_t rpId{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[4]{};
        rootParameters[rpId++].InitAsConstants(4, 0);
        rootParameters[rpId++].InitAsShaderResourceView(0);
        rootParameters[rpId++].InitAsShaderResourceView(1);
        rootParameters[rpId++].InitAsUnorderedAccessView(0);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters);

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->GetD3D12Device()->CheckFeatureSupport(
            D3D12_FEATURE_ROOT_SIGNATURE,
            &featureData,
            sizeof(featureData)
        ))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
        HRESULT hr{ D3DX12SerializeVersionedRootSignature(
            &rootSignatureDescription,
            featureData.HighestVersion,
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