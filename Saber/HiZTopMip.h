#pragma once

#include "ComputeObject.h"
#include "DeviceContext.h"

// Fills mip 0 of the hierarchical depth buffer from the depth buffer, resampling it
// onto the square power of two pyramid
class HiZTopMip {
public:
    static constexpr UINT GroupSize{ 8 };

    static std::shared_ptr<ComputeObject> Create(
        std::shared_ptr<DeviceContext> pDeviceContext
    ) {
        std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
        pComputeObj->InitMaterial(
            pDeviceContext,
            ComputeObject::RootSignatureData{
                CreateRootSignatureBlob(),
                L"HiZTopMipRootSignature"
            },
            ComputeObject::ComputeShaderData{ L"HiZTopMipCS.cso" }
        );

        return pComputeObj;
    }

private:
    static Microsoft::WRL::ComPtr<D3DBlob> CreateRootSignatureBlob() {
        size_t rp{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[3]{};

        // source size, target size
        rootParameters[rp++].InitAsConstants(4, 0);

        // depth buffer
        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvDepth[1]{};
        rangeSrvDepth[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeSrvDepth), rangeSrvDepth);

        // hierarchical depth buffer mip 0
        CD3DX12_DESCRIPTOR_RANGE1 rangeUavTopMip[1]{};
        rangeUavTopMip[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeUavTopMip), rangeUavTopMip);

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
