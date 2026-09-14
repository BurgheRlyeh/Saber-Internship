#pragma once

#include "ComputeObject.h"
#include "DeviceContext.h"

// Two pass occlusion culling: the same shader runs twice per frame, the pass it is
// in comes from CullingParams::flags
class OcclusionCulling {
public:
    static std::shared_ptr<ComputeObject> CreateConstMesh4Culling(
        std::shared_ptr<DeviceContext> pDeviceContext
    ) {
        return Create(pDeviceContext, L"OcclusionCullingConstMesh4.cso");
    }

private:
    static std::shared_ptr<ComputeObject> Create(
        std::shared_ptr<DeviceContext> pDeviceContext,
        const std::wstring& filename
    ) {
        std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
        pComputeObj->InitMaterial(
            pDeviceContext,
            ComputeObject::RootSignatureData{
                CreateRootSignatureBlob(),
                L"OcclusionCullingRootSignature"
            },
            ComputeObject::ComputeShaderData{ filename }
        );

        return pComputeObj;
    }

    static Microsoft::WRL::ComPtr<D3DBlob> CreateRootSignatureBlob() {
        size_t rp{}, srCbv{}, srSrv{}, srUav{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[9]{};

        rootParameters[rp++].InitAsConstants(4, srCbv++);           // culling params
        rootParameters[rp++].InitAsConstantBufferView(srCbv++);     // culling camera

        rootParameters[rp++].InitAsShaderResourceView(srSrv++);     // model buffers
        rootParameters[rp++].InitAsShaderResourceView(srSrv++);     // source commands

        rootParameters[rp++].InitAsUnorderedAccessView(srUav++);    // culled commands
        rootParameters[rp++].InitAsUnorderedAccessView(srUav++);    // culled commands count
        rootParameters[rp++].InitAsUnorderedAccessView(srUav++);    // visibility
        rootParameters[rp++].InitAsUnorderedAccessView(srUav++);    // stats of this subsystem

        // A texture cannot be a root descriptor, unlike everything above
        CD3DX12_DESCRIPTOR_RANGE1 rangeSrvHzb[1]{};
        rangeSrvHzb[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, srSrv++);
        rootParameters[rp++].InitAsDescriptorTable(_countof(rangeSrvHzb), rangeSrvHzb);

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
