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
    m_pSpdCounterBuffer = std::make_shared<Texture>(
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
            pCommandListCompute->SetComputeRootDescriptorTable(1, srvHandle);
            pCommandListCompute->SetComputeRootDescriptorTable(3, midMipUavHandle);
            pCommandListCompute->SetComputeRootDescriptorTable(4, mipsUavsHandle);
        }
    );
}

void SinglePassDownsampler::InnerRootParametersSetter(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    UINT& rootParamId
) const {
    pCommandListDirect->SetComputeRootDescriptorTable(0, m_pSpdConstantBufferRange->GetGpuHandle());
    pCommandListDirect->SetComputeRootDescriptorTable(2, m_pSpdCounterBufferRange->GetGpuHandle());
}
