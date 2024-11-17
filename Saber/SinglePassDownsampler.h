#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "ComputeObject.h"
#include "ConstantBuffer.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"
#include "Texture.h"

#define FFX_CPU
#include "FidelityFX/gpu/ffx_core.h"
#include "FidelityFX/gpu/spd/ffx_spd.h"

class SinglePassDownsampler : public ComputeObject {
    struct SpdGlobalAtomicBuffer {
        FfxUInt32 counter[6];
    };
    std::shared_ptr<GPUResource> m_pSpdCounterBuffer{};
    std::shared_ptr<DescHeapRange> m_pSpdCounterBufferRange{};

    struct SPDConstantBuffer {
        FfxUInt32       mips{};
        FfxUInt32       numWorkGroups{};
        FfxUInt32x2     workGroupOffset{};
        FfxFloat32x2    invInputSize{};     // Only used for linear sampling mode
        FfxFloat32x2    padding{};
    } m_spdConstantBuffer{};
    std::shared_ptr<ConstantBuffer> m_pSpdConstantBuffer{};
    std::shared_ptr<DescHeapRange> m_pSpdConstantBufferRange{};

    uint32_t m_dispatchX{};
    uint32_t m_dispatchY{};

public:
    SinglePassDownsampler(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary,
        std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
        UINT64 width,
        UINT height
    );

    void Resize(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        UINT64 width,
        UINT height
    );

    void Dispatch(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap,
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE midMipUavHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE mipsUavsHandle
    );

protected:
    virtual void InnerRootParametersSetter(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        UINT& rootParamId
    ) const override;

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    );
};