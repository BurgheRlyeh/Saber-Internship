#pragma once

#include "ComputeObject.h"
#include "DeviceContext.h"

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
                CreateRootSignatureBlob(),
                L"IndirectUpdaterRootSignature"
            },
            ComputeShaderData{ filename }
        );

        return pComputeObj;
    }

    static Microsoft::WRL::ComPtr<D3DBlob> CreateRootSignatureBlob() {
        size_t rpId{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[4]{};
        rootParameters[rpId++].InitAsConstants(4, 0);
        rootParameters[rpId++].InitAsShaderResourceView(0);
        rootParameters[rpId++].InitAsShaderResourceView(1);
        rootParameters[rpId++].InitAsUnorderedAccessView(0);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters);

        // Serialize the root signature.
        Microsoft::WRL::ComPtr<D3DBlob> rootSignatureBlob, errorBlob;
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