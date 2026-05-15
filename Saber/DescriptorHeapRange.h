/**
 * @file DescriptorHeapRange.h
 * @brief A contiguous slice of descriptors within a @ref DescriptorHeapManager heap.
 */
#pragma once

#include "Headers.h"

#include <optional>

/**
 * @brief Identifies the category of a descriptor range.
 *
 * @c ResNumTypes (5) covers only the resource-view types (SRV, UAV, CBV, RTV, DSV);
 * @c NumTypes (6) includes samplers.
 */
enum class DescRangeType : uint8_t {
    Srv = 0, /**< @brief Shader resource view. */
    Uav = 1, /**< @brief Unordered access view. */
    Cbv = 2, /**< @brief Constant buffer view. */
    Rtv = 3, /**< @brief Render target view. */
    Dsv = 4, /**< @brief Depth-stencil view. */
    Smp = 5, /**< @brief Sampler. */
    NumTypes    = 6, /**< @brief Total number of descriptor types including samplers. */
    ResNumTypes = 5  /**< @brief Number of resource-view types (excludes samplers). */
};

/**
 * @brief Returns the wide-string name of a @ref DescRangeType value.
 * @param type Descriptor range type.
 * @return Human-readable name, e.g. @c L"Srv".
 */
constexpr std::wstring ToName(DescRangeType type) {
    switch (type) {
    case DescRangeType::Srv: return L"Srv";
    case DescRangeType::Uav: return L"Uav";
    case DescRangeType::Cbv: return L"Cbv";
    case DescRangeType::Rtv: return L"Rtv";
    case DescRangeType::Dsv: return L"Dsv";
    case DescRangeType::Smp: return L"Smp";
    default: return L"";
    }
}

/**
 * @brief A contiguous slice of descriptors inside a descriptor heap.
 *
 * Stores the CPU and GPU base handles, the handle increment size, capacity,
 * and a current-size cursor so that descriptors can be appended sequentially
 * via @ref GetNextCpuHandle.
 */
class DescRange {
    const std::wstring m_name{};

    UINT m_handleIncSize{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle{};
    std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> m_type{};

    size_t m_size{};     /**< @brief Number of descriptors currently written (cursor). */
    size_t m_capacity{}; /**< @brief Maximum number of descriptors in this range. */

public:
    DescRange() = delete;
    DescRange(const DescRange& other) = default;

    /**
     * @brief Constructs a range from explicit heap handles.
     * @param name         Debug name.
     * @param capacity     Maximum descriptor count.
     * @param handleIncSize Byte stride between consecutive descriptors.
     * @param cpuHandle    CPU handle of the first descriptor in this range.
     * @param gpuHandle    GPU handle of the first descriptor in this range.
     * @param type         Optional D3D12 range type for validation.
     */
    DescRange(
        const std::wstring& name,
        const size_t& capacity,
        const UINT& handleIncSize,
        const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
        const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle,
        const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE>& type = std::nullopt
    );

    /**
     * @brief Constructs a named alias that shares the same handles and capacity as @p other.
     * @param name  New debug name.
     * @param other Source range to copy handles from.
     */
    DescRange(
        const std::wstring& name,
        const DescRange& other
    );

    /**
     * @brief Returns the CPU handle at index @p id within this range.
     * @param id Zero-based descriptor index (default 0).
     * @return CPU descriptor handle offset by @p id increments.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t id = 0) const;

    /**
     * @brief Returns the GPU handle at index @p id within this range.
     * @param id Zero-based descriptor index (default 0).
     * @return GPU descriptor handle offset by @p id increments.
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t id = 0) const;

    /** @brief Returns the number of descriptors currently written into this range. */
    size_t GetSize() const;

    /**
     * @brief Returns the next free descriptor index and advances the internal cursor.
     * @return Index of the newly reserved slot.
     */
    size_t GetNextId();

    /**
     * @brief Returns the CPU handle of the next free descriptor and advances the cursor.
     * @return CPU handle pointing to the reserved slot.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetNextCpuHandle();

    /** @brief Resets the write cursor to zero (descriptors are not cleared on the heap). */
    void Clear();
};
