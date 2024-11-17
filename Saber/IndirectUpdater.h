#pragma once

#include "Atlas.h"
#include "ComputeObject.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"

class IndirectUpdater : ComputeObject {
public:
    static std::shared_ptr<ComputeObject> CreateCbMeshUpdater(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        return Create(
            pDevice,
            pShaderAtlas,
            pRootSignatureAtlas,
            pPSOLibrary,
            L"CbMeshIndirectUpdater.cso"
        );
    }
     
    static std::shared_ptr<ComputeObject> CreateCbMesh4Updater(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        return Create(
            pDevice,
            pShaderAtlas,
            pRootSignatureAtlas,
            pPSOLibrary,
            L"CbMesh4IndirectUpdater.cso"
        );
    }

private:
    static std::shared_ptr<ComputeObject> Create(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        const LPCWSTR& filename
    ) {
        std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
        pComputeObj->InitMaterial(
            pDevice,
            RootSignatureData(
                pRootSignatureAtlas,
                CreateRootSignatureBlob(pDevice),
                L"IndirectUpdaterRootSignature"
            ),
            ComputeShaderData(pShaderAtlas, filename),
            pPSOLibrary
        );

        return pComputeObj;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    ) {
        size_t rpId{};
        CD3DX12_ROOT_PARAMETER1 rootParameters[4]{};
        rootParameters[rpId++].InitAsConstants(1, 0);
        rootParameters[rpId++].InitAsShaderResourceView(0);
        rootParameters[rpId++].InitAsShaderResourceView(1);

        CD3DX12_DESCRIPTOR_RANGE1 rangeUavIndirect[1]{};
        rangeUavIndirect[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        rootParameters[rpId++].InitAsDescriptorTable(
            _countof(rangeUavIndirect),
            rangeUavIndirect
        );

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters);

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
        if (FAILED(pDevice->CheckFeatureSupport(
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