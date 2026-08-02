#pragma once

#include "Headers.h"

#include "ComputeObject.h"

template <typename T>
class Buffer;
class CommandList;
class DescRange;
class Device;
class DeviceContext;
class GPUResource;

class SinglePassDownsampler : public ComputeObject {
    static const std::wstring BASE_NAME;

    struct SpdGlobalAtomicBuffer {
        uint32_t counter[6]{};
    };
    std::shared_ptr<Buffer<SpdGlobalAtomicBuffer>> m_pSpdCounterBuffer{};

    struct SPDConstantBuffer {
        uint32_t mips{};
        uint32_t numWorkGroups{};
        uint32_t workGroupOffset[2]{};
        float invInputSize[2]{};     // Only used for linear sampling mode
        float padding[2]{};
	} m_spdConstantBuffer{};
	std::shared_ptr<Buffer<SPDConstantBuffer>> m_pSpdConstantBuffer{};

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
        Microsoft::WRL::ComPtr<D3D12DescriptorHeap> pDescHeap,
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
    static Microsoft::WRL::ComPtr<D3DBlob> CreateRootSignatureBlob();
};