/**
 * @file DeviceContext.h
 * @brief Central context object that aggregates all per-device GPU resources
 *        (command queues, descriptor heaps, atlases, ring buffers, etc.).
 */
#pragma once

#include "Headers.h"

#include <array>

#include "Atlas.h"
#include "DynamicUploadRingBuffer.h"
#include "FencedQueue.h"

class CommandQueue;
class DescriptorHeapManager;
class Device;
template <typename T>
class FrameDataBuffer;
class GPUResource;
class MaterialManager;
class Mesh;
class PSOLibrary;

struct RootSignatureResource;
struct ShaderResource;

/**
 * @brief Owns and provides access to all device-level rendering resources.
 *
 * Subsystems obtain shared pointers to command queues, descriptor heaps,
 * atlases (meshes, root signatures, shaders), the PSO library, and ring
 * buffers through this single context object.  The intermediate-resource
 * list (@c AddIntermediate / @c FinishFrame) ensures that upload buffers
 * are kept alive until the GPU has consumed them.
 */
class DeviceContext {
    std::wstring m_name{};

    std::shared_ptr<Device> m_pDevice{};

    // Command Queues
    std::shared_ptr<CommandQueue> m_pCommandQueueDirect{};
    std::shared_ptr<CommandQueue> m_pCommandQueueCompute{};
    std::shared_ptr<CommandQueue> m_pCommandQueueCopy{};

    std::array<std::shared_ptr<DescriptorHeapManager>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_pDescHeaps{};

    // Atlases
    std::shared_ptr<Atlas<RootSignatureResource>> m_pRootSignatureAtlas{};
    std::shared_ptr<Atlas<ShaderResource>> m_pShaderAtlas{};
    std::shared_ptr<PSOLibrary> m_pPSOLibrary{};

    std::shared_ptr<Atlas<Mesh>> m_pMeshAtlas{};
    std::shared_ptr<MaterialManager> m_pMaterialManager{};

    // Ring Buffers
    std::array<std::shared_ptr<DynamicUploadHeap>, static_cast<size_t>(RingBufferType::Count)> m_pRingBuffers{};
    std::shared_ptr<FrameDataBuffer<std::shared_ptr<GPUResource>>> m_pFrameDataBuffer{};

public:
    /** @brief Per-descriptor-heap initialisation arguments. */
    struct DescHeapArgs {
        size_t size{};                      /**< @brief Number of descriptors in the heap. */
        D3D12_DESCRIPTOR_HEAP_FLAGS flags{}; /**< @brief Heap visibility flags. */
    };

    /**
     * @brief Creates the D3D12 device for the given adapter.
     * @param pAdapter DXGI adapter to use.
     */
    DeviceContext(Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter);

    ~DeviceContext();

    /**
     * @brief Completes context initialisation: creates queues, descriptor heaps,
     *        atlases, PSO library, and ring buffers.
     * @param descHeapArgs Per-type heap size and flag configuration.
     */
    void InitializeContext(
        const std::array<DescHeapArgs, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& descHeapArgs
    );

    /** @brief Returns the logical D3D12 @ref Device. */
    std::shared_ptr<Device> GetDevice() const {
        return m_pDevice;
    }

    /**
     * @brief Returns the command queue of the specified type.
     * @param type Queue type (Direct, Compute, or Copy; default Direct).
     * @return Shared pointer to the command queue.
     * @throws std::runtime_error for unsupported types.
     */
    std::shared_ptr<CommandQueue> GetCommandQueue(
        const D3D12_COMMAND_LIST_TYPE& type = D3D12_COMMAND_LIST_TYPE_DIRECT
    ) const {
        switch (type) {
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            return m_pCommandQueueDirect;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            return m_pCommandQueueCompute;
        case D3D12_COMMAND_LIST_TYPE_COPY:
            return m_pCommandQueueCopy;
        default:
            throw std::runtime_error("Unsupported CommandQueue type");
        }
    }

    /**
     * @brief Returns the descriptor heap manager for the given heap type.
     * @param type Heap type (default CBV/SRV/UAV).
     * @return Shared pointer to the descriptor heap manager.
     */
    std::shared_ptr<DescriptorHeapManager> GetDescriptorHeap(
        const D3D12_DESCRIPTOR_HEAP_TYPE& type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    ) const {
        assert(0 <= type && type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
        return m_pDescHeaps[type];
    }

    /** @brief Returns the atlas of cached @ref RootSignatureResource objects. */
    std::shared_ptr<Atlas<RootSignatureResource>> GetRootSignatureAtlas() const {
        return m_pRootSignatureAtlas;
    }

    /** @brief Returns the atlas of cached @ref ShaderResource objects. */
    std::shared_ptr<Atlas<ShaderResource>> GetShaderAtlas() const {
        return m_pShaderAtlas;
    }

    /** @brief Returns the pipeline-state object library (disk-backed PSO cache). */
    std::shared_ptr<PSOLibrary> GetPSOLibrary() const {
        return m_pPSOLibrary;
    }

    /** @brief Returns the atlas of cached @ref Mesh objects. */
    std::shared_ptr<Atlas<Mesh>> GetMeshAtlas() const {
        return m_pMeshAtlas;
    }

    /** @brief Stores a reference to the scene's material manager. */
    void SetMaterialManager(std::shared_ptr<MaterialManager> pMaterialManager) {
        m_pMaterialManager = pMaterialManager;
    }

    /** @brief Returns the scene's material manager. */
    std::shared_ptr<MaterialManager> GetMaterialManager() const {
        return m_pMaterialManager;
    }

    /**
     * @brief Returns the dynamic upload ring buffer of the specified type.
     * @param type Ring-buffer category (CPU or GPU; default CPU).
     * @return Shared pointer to the ring buffer.
     */
    std::shared_ptr<DynamicUploadHeap> GetRingBuffer(
        const RingBufferType& type = RingBufferType::CPU
    ) const {
        return m_pRingBuffers[static_cast<size_t>(type)];
    }

    /**
     * @brief Registers a GPU resource to be kept alive until the current frame completes.
     *
     * Typically used for upload intermediate buffers whose lifetime must extend
     * past the CPU-side record phase until the GPU finishes reading them.
     *
     * @param pResource Resource to retain.
     */
    void AddIntermediate(std::shared_ptr<GPUResource> pResource) {
        m_pFrameDataBuffer->Add(pResource);
    }

    /**
     * @brief Advances the per-frame buffer and ring-buffer trackers.
     *
     * Call once per frame after submitting all GPU work for this frame.
     *
     * @param fenceValue             Fence value that will be signalled when this frame ends.
     * @param lastCompletedFenceValue Highest fence value the GPU has already completed.
     */
    void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
        for (auto& pRingBuffer : m_pRingBuffers) {
            pRingBuffer->FinishFrame(fenceValue, lastCompletedFenceValue);
        }
        m_pFrameDataBuffer->FinishFrame(fenceValue, lastCompletedFenceValue);
    }

private:
    /**
     * @brief Configures the D3D12 info-queue message filter to suppress irrelevant warnings.
     * @param pDevice Device whose info-queue to configure.
     */
    static void SetInfoQueueFilter(Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice);
};
