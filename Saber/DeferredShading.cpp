#include "DeferredShading.h"

#include "DeviceContext.h"

std::shared_ptr<ComputeObject> DeferredShading::CreateDefferedShadingComputeObject(
    std::shared_ptr<DeviceContext> pDeviceContext,
    bool DoSSAO
) {
    std::shared_ptr<ComputeObject> pComputeObj{ std::make_shared<ComputeObject>() };
    pComputeObj->InitMaterial(
        pDeviceContext,
        RootSignatureData(
            CreateRootSignatureBlob(DoSSAO),
            L"DeferredShadingRootSignature"
        ),
        ComputeShaderData(
            DoSSAO ? L"DeferredShadingVBAO_CS.cso" : L"DeferredShadingComputeShader.cso"
        )
    );

    return pComputeObj;
}

Microsoft::WRL::ComPtr<ID3DBlob> DeferredShading::CreateRootSignatureBlob(bool DoSSAO) {
    size_t rpId{};
    CD3DX12_ROOT_PARAMETER1 rootParameters[7]{};
    rootParameters[rpId++].InitAsConstantBufferView(0);
    rootParameters[rpId++].InitAsConstantBufferView(1);

    CD3DX12_DESCRIPTOR_RANGE1 rangeSrvsGbuffer[2]{};
    rangeSrvsGbuffer[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
    rangeSrvsGbuffer[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 3);
    rootParameters[rpId++].InitAsDescriptorTable(DoSSAO ? _countof(rangeSrvsGbuffer) : 1, rangeSrvsGbuffer);

    CD3DX12_DESCRIPTOR_RANGE1 rangeUavsGbuffer[1]{};
	rangeUavsGbuffer[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	    rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeUavsGbuffer), rangeUavsGbuffer);

    CD3DX12_DESCRIPTOR_RANGE1 rangeDepthBuffer[1]{};
    rangeDepthBuffer[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeDepthBuffer), rangeDepthBuffer);

    CD3DX12_DESCRIPTOR_RANGE1 rangeCbvsMaterials[1]{};
    rangeCbvsMaterials[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
    rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeCbvsMaterials), rangeCbvsMaterials);

    CD3DX12_DESCRIPTOR_RANGE1 rangeSrvsMaterial[1]{};
    rangeSrvsMaterial[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    rootParameters[rpId++].InitAsDescriptorTable(_countof(rangeSrvsMaterial), rangeSrvsMaterial);

    D3D12_STATIC_SAMPLER_DESC sampler{
        .Filter{ D3D12_FILTER_ANISOTROPIC },
        .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
        .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
        .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
        .MipLODBias{},
        .MaxAnisotropy{ 16 },
        .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
        .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
        .MinLOD{},
        .MaxLOD{ D3D12_FLOAT32_MAX },
        .ShaderRegister{},
        .RegisterSpace{},
        .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
    };

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler);

    // Serialize the root signature.
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
        &rootSignatureDescription,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &rootSignatureBlob,
        &errorBlob
    ));

    return rootSignatureBlob;
}
