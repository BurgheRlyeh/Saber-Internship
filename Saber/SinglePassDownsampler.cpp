#include "SinglePassDownsampler.h"

SinglePassDownsampler::SinglePassDownsampler(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
    std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
    std::shared_ptr<PSOLibrary> pPSOLibrary,
    std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
    UINT64 width,
    UINT height
) {
    InitMaterial(
        pDevice,
        RootSignatureData(
            pRootSignatureAtlas,
            CreateRootSignatureBlob(pDevice),
            L"AmdFfxSpdDownsamplePassRootSignature"
        ),
        ComputeShaderData(
            pShaderAtlas,
            L"SinglePassDownsamplerCS.cso"
        ),
        pPSOLibrary
    );

    m_pSpdCounterBufferRange = pDescHeapManagerCbvSrvUav->AllocateRange(
        L"SPDCounterBuffer", 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV
    );
    m_pSpdConstantBufferRange = pDescHeapManagerCbvSrvUav->AllocateRange(
        L"SPDConstantBuffer", 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV
    );

    Resize(pDevice, pAllocator, width, height);
}

void SinglePassDownsampler::Resize(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    UINT64 width,
    UINT height
) {
    m_pSpdCounterBufferRange->Clear();
    m_pSpdConstantBufferRange->Clear();

    // global atomic counter buffer
    m_pSpdCounterBuffer = std::make_shared<GPUResource>(
        pAllocator,
        GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
        GPUResource::ResourceData{
            .resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
                sizeof(SpdGlobalAtomicBuffer),
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            ) },
            .resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
        }
    );
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
        .ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
        .Buffer{ .NumElements{ 6 }, .StructureByteStride{ sizeof(FfxUInt32) }, }
    };
    m_pSpdCounterBuffer->CreateUnorderedAccessView(
        pDevice,
        m_pSpdCounterBufferRange->GetNextCpuHandle(),
        &uavDesc
    );

    // spd constant buffer
    FfxUInt32x2 dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips;
    FfxUInt32x4 rectInfo{ 0, 0, width, height };
    ffxSpdSetup(
        dispatchThreadGroupCountXY,
        workGroupOffset,
        numWorkGroupsAndMips,
        rectInfo
    );
    m_dispatchX = dispatchThreadGroupCountXY[0];
    m_dispatchY = dispatchThreadGroupCountXY[1];
    m_spdConstantBuffer.mips = numWorkGroupsAndMips[1];
    m_spdConstantBuffer.numWorkGroups = numWorkGroupsAndMips[0];
    m_spdConstantBuffer.workGroupOffset[0] = workGroupOffset[0];
    m_spdConstantBuffer.workGroupOffset[1] = workGroupOffset[1];
    m_pSpdConstantBuffer = std::make_shared<ConstantBuffer>(
        pAllocator,
        sizeof(SPDConstantBuffer),
        &m_spdConstantBuffer
    );
    m_pSpdConstantBuffer->CreateConstantBufferView(pDevice, m_pSpdConstantBufferRange->GetNextCpuHandle());
}

void SinglePassDownsampler::Dispatch(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute,
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap,
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE midMipUavHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE mipsUavsHandle
) {
    ComputeObject::Dispatch(
        pCommandListCompute,
        m_dispatchX,
        m_dispatchY,
        1,
        [&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute, UINT& rootParamId) {
            pCommandListCompute->SetDescriptorHeaps(1, pDescHeap.GetAddressOf());
            pCommandListCompute->SetComputeRootDescriptorTable(0, srvHandle);
            pCommandListCompute->SetComputeRootDescriptorTable(2, midMipUavHandle);
            pCommandListCompute->SetComputeRootDescriptorTable(3, mipsUavsHandle);
        }
    );
}

void SinglePassDownsampler::InnerRootParametersSetter(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    UINT& rootParamId
) const {
    pCommandListDirect->SetComputeRootDescriptorTable(1, m_pSpdCounterBufferRange->GetGpuHandle());
    pCommandListDirect->SetComputeRootDescriptorTable(4, m_pSpdConstantBufferRange->GetGpuHandle());
}

Microsoft::WRL::ComPtr<ID3DBlob> SinglePassDownsampler::CreateRootSignatureBlob(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
) {
    size_t rp{};
    CD3DX12_ROOT_PARAMETER1 rootParameters[5]{};

    // input texture
    CD3DX12_DESCRIPTOR_RANGE1 rangeSrvInput[1]{};
    rangeSrvInput[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    rootParameters[rp++].InitAsDescriptorTable(_countof(rangeSrvInput), rangeSrvInput);

    // global atomic counter
    CD3DX12_DESCRIPTOR_RANGE1 rangeUavCounter[1]{};
    rangeUavCounter[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    rootParameters[rp++].InitAsDescriptorTable(_countof(rangeUavCounter), rangeUavCounter);

    // mid mipmap
    CD3DX12_DESCRIPTOR_RANGE1 rangeUavMidMipmap[1]{};
    rangeUavMidMipmap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
    rootParameters[rp++].InitAsDescriptorTable(_countof(rangeUavMidMipmap), rangeUavMidMipmap);

    // mips
    CD3DX12_DESCRIPTOR_RANGE1 rangeUavMips[1]{};
    rangeUavMips[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
    rootParameters[rp++].InitAsDescriptorTable(_countof(rangeUavMips), rangeUavMips);

    // SPD ConstantBuffer
    CD3DX12_DESCRIPTOR_RANGE1 rangeCbvSpd[1]{};
    rangeCbvSpd[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    rootParameters[rp++].InitAsDescriptorTable(_countof(rangeCbvSpd), rangeCbvSpd);

    D3D12_STATIC_SAMPLER_DESC sampler{
        .Filter{ D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT },
        .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
        .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
        .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
        .MipLODBias{},
        .MaxAnisotropy{ 1 },
        .ComparisonFunc{ D3D12_COMPARISON_FUNC_NEVER },
        .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
        .MinLOD{},
        .MaxLOD{ D3D12_FLOAT32_MAX },
        .ShaderRegister{},
        .RegisterSpace{},
        .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
    };

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler);

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
