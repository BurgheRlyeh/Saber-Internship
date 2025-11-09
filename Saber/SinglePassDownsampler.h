#pragma once

#include "Headers.h"

#include "DeviceContext.h"
#include "Atlas.h"
#include "ComputeObject.h"
#include "ConstantBuffer.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"
#include "TextureResource.h"

#define FFX_CPU
#include "FidelityFX/gpu/ffx_core.h"
#include "FidelityFX/gpu/spd/ffx_spd.h"

class SinglePassDownsampler : public ComputeObject {
    static const std::wstring BASE_NAME;

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
        std::shared_ptr<DeviceContext> pDeviceContext,
        UINT64 width,
        UINT height
    );

    void Resize(
        std::shared_ptr<Device> pDevice,
        UINT64 width,
        UINT height
    );

    void Dispatch(
        std::shared_ptr<CommandList> pCommandList,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap,
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE midMipUavHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE mipsUavsHandle
    );

protected:
    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const override;

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob(
        std::shared_ptr<Device> pDevice
    );
};