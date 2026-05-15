/**
 * @file SinglePassDownsampler.h
 * @brief GPU hierarchical downsampler based on the AMD FidelityFX SPD algorithm.
 *
 * @ref SinglePassDownsampler extends @ref ComputeObject with the SPD-specific
 * global atomic counter buffer and constant buffer.  A single @ref Dispatch call
 * generates all mip levels of a 2-D texture in one compute pass without
 * inter-dispatch synchronisation.
 *
 * @note Reference: https://gpuopen.com/fidelityfx-spd/
 */
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

/**
 * @brief Single-pass hierarchical downsampler for generating a full mip chain.
 *
 * Owns the SPD global atomic counter buffer (@ref SpdGlobalAtomicBuffer) and the
 * SPD constant buffer (@ref SPDConstantBuffer) that carry per-dispatch parameters
 * such as mip count, work-group count, and inverse input size.
 */
class SinglePassDownsampler : public ComputeObject {
    static const std::wstring BASE_NAME;

    /** @brief Per-slice atomic counters required by the SPD algorithm. */
    struct SpdGlobalAtomicBuffer {
        uint32_t counter[6]{};
    };
    std::shared_ptr<Buffer<SpdGlobalAtomicBuffer>> m_pSpdCounterBuffer{};

    /** @brief Per-dispatch SPD parameters uploaded as a root constant buffer. */
    struct SPDConstantBuffer {
        uint32_t mips{};                 /**< @brief Number of mip levels to generate. */
        uint32_t numWorkGroups{};        /**< @brief Total number of compute work groups. */
        uint32_t workGroupOffset[2]{};   /**< @brief Work-group offset for tiled dispatches. */
        float invInputSize[2]{};         /**< @brief Reciprocal of the input texture dimensions (linear-sampling mode). */
        float padding[2]{};
    } m_spdConstantBuffer{};
    std::shared_ptr<Buffer<SPDConstantBuffer>> m_pSpdConstantBuffer{};

    uint32_t m_dispatchX{}; /**< @brief Dispatch thread group count in X. */
    uint32_t m_dispatchY{}; /**< @brief Dispatch thread group count in Y. */

public:
    /**
     * @brief Constructs the downsampler and allocates GPU resources.
     * @param pDeviceContext Device context.
     * @param width          Input texture width in texels.
     * @param height         Input texture height in texels.
     */
    SinglePassDownsampler(
        std::shared_ptr<DeviceContext> pDeviceContext,
        UINT64 width,
        UINT height
    );

    /**
     * @brief Recalculates dispatch dimensions and SPD constants for a new resolution.
     * @param pDevice Device wrapper (for buffer re-creation if needed).
     * @param width   New input texture width.
     * @param height  New input texture height.
     */
    void Resize(
        std::shared_ptr<Device> pDevice,
        UINT64 width,
        UINT height
    );

    /**
     * @brief Dispatches the SPD compute pass to generate all mip levels.
     * @param pCommandList       Compute or direct command list.
     * @param pDescHeap          Descriptor heap containing the SRV and UAV descriptors.
     * @param srvHandle          GPU handle to the source texture SRV (mip 0).
     * @param midMipUavHandle    GPU handle to the mid-level UAV (mip 6, used as the inter-wave rendezvous).
     * @param mipsUavsHandle     GPU handle to the full UAV table for all output mip levels.
     */
    void Dispatch(
        std::shared_ptr<CommandList> pCommandList,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap,
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE midMipUavHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE mipsUavsHandle
    );

protected:
    /**
     * @brief Binds the SPD root parameters: counter UAV, constant buffer, and sampler.
     * @param pCommandList Command list.
     * @param rootParamId  In/out root parameter index; incremented for each binding.
     */
    virtual void InnerRootParametersSetter(
        std::shared_ptr<CommandList> pCommandList,
        UINT& rootParamId
    ) const override;

private:
    /** @brief Creates the serialised SPD root signature blob. */
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob();
};
