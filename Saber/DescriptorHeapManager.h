/**
 * @file DescriptorHeapManager.h
 * @brief Owns a D3D12 descriptor heap and sub-allocates named ranges from it.
 */
#pragma once

#include "Headers.h"

#include <optional>

#include "Atlas.h"

class DescRange;
class Device;

/**
 * @brief Manages a single D3D12 descriptor heap and sub-allocates @ref DescRange
 *        slices from it on demand.
 *
 * Ranges are cached in an internal @ref Atlas so that the same named range is
 * not allocated twice.  Call @ref AllocateRange to obtain or create a contiguous
 * slice of descriptors, and @ref GetRange to look one up by name.
 */
class DescriptorHeapManager {
    D3D12_DESCRIPTOR_HEAP_DESC m_heapDesc{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescHeap{};
    UINT m_handleIncSize{}; /**< @brief Byte increment between consecutive descriptor handles. */

    std::shared_ptr<Atlas<DescRange>> m_pRangesAtlas{}; /**< @brief Cache of allocated ranges keyed by name. */

    size_t m_firstFreeId{}; /**< @brief Next free descriptor slot index. */

public:
    /**
     * @brief Creates the descriptor heap from an explicit descriptor.
     * @param name     Debug name.
     * @param pDevice  Device on which to create the heap.
     * @param heapDesc Full heap descriptor (type, size, flags).
     */
    DescriptorHeapManager(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        const D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
    );

    /**
     * @brief Convenience constructor that builds the @c D3D12_DESCRIPTOR_HEAP_DESC internally.
     * @param name   Debug name.
     * @param pDevice Device.
     * @param type   Descriptor heap type (CBV/SRV/UAV, RTV, DSV, or Sampler).
     * @param size   Number of descriptors in the heap.
     * @param flags  Heap flags (default none; use @c SHADER_VISIBLE for CBV/SRV/UAV heaps).
     */
    DescriptorHeapManager(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        const D3D12_DESCRIPTOR_HEAP_TYPE& type,
        const size_t& size,
        const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    ) : DescriptorHeapManager(
            name,
            pDevice,
            D3D12_DESCRIPTOR_HEAP_DESC{ type, static_cast<UINT>(size), flags }
        )
    {}

    /** @brief Returns the underlying @c ID3D12DescriptorHeap COM pointer. */
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() const;

    /**
     * @brief Sub-allocates a contiguous range of @p size descriptors and caches it by @p name.
     *
     * If a range with the same name already exists in the atlas, a slice aliasing
     * its start position is returned instead of allocating new descriptors.
     *
     * @param name  Unique name for the range (used as atlas key).
     * @param size  Number of descriptors to allocate.
     * @param type  Optional descriptor-range type hint stored in the range.
     * @return Shared pointer to the allocated @ref DescRange.
     */
    std::shared_ptr<DescRange> AllocateRange(
        const std::wstring& name,
        const size_t& size,
        const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE>& type = std::nullopt
    );

    /**
     * @brief Looks up a previously allocated range by name.
     * @param name Name passed to @ref AllocateRange.
     * @return Shared pointer to the range, or empty if not found.
     */
    std::shared_ptr<DescRange> GetRange(const std::wstring& name);
};
