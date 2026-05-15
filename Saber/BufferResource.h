/**
 * @file BufferResource.h
 * @brief Typed GPU buffer resource with automatic view-descriptor generation.
 */
#pragma once

#include "Headers.h"

#include "GPUResource.h"

/**
 * @brief A typed GPU buffer that wraps @ref GPUResource and provides
 *        element-count-aware SRV, UAV, CBV, and RTV descriptors.
 *
 * The underlying D3D12 resource is sized to @p capacity elements of type @p T,
 * rounded up to @c D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT bytes.
 *
 * @tparam T Element type stored in the buffer.
 */
template <typename T>
class BufferResource : public GPUResource {
public:
    /**
     * @brief Allocates the GPU buffer on construction.
     * @param name      Debug name.
     * @param pDevice   Device and allocator.
     * @param capacity  Number of @p T elements to allocate (default 1).
     * @param allocDesc Heap type and allocation flags.
     * @param resDesc   Resource description override; width is overwritten with the aligned byte size.
     */
    BufferResource(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        size_t capacity = 1,
        const AllocationDesc& allocDesc = {},
        ResourceDesc& resDesc = { CD3DX12_RESOURCE_DESC::Buffer(0) }
    ) {
        CreateResource(name, pDevice, capacity, allocDesc, resDesc);
    }

    /**
     * @brief Creates (or re-creates) the D3D12 resource for @p capacity elements.
     *
     * Asserts that @p resDesc describes a 1-D row-major buffer without MSAA or mips.
     *
     * @param name      Debug name.
     * @param pDevice   Device and allocator.
     * @param capacity  Number of @p T elements.
     * @param allocDesc Heap parameters.
     * @param resDesc   Base resource description; @c Width is set internally.
     */
    void CreateResource(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        size_t capacity = 1,
        const AllocationDesc& allocDesc = {},
        ResourceDesc& resDesc = { CD3DX12_RESOURCE_DESC::Buffer(0) }
    ) {
        assert(resDesc.resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
            && resDesc.resDesc.Height == 1
            && resDesc.resDesc.DepthOrArraySize == 1
            && resDesc.resDesc.MipLevels == 1
            && resDesc.resDesc.Format == DXGI_FORMAT_UNKNOWN
            && resDesc.resDesc.SampleDesc.Count == 1
            && resDesc.resDesc.SampleDesc.Quality == 0
            && resDesc.resDesc.Layout == D3D12_TEXTURE_LAYOUT_ROW_MAJOR);
        assert(resDesc.pResClearValue == nullptr);
        resDesc.resDesc.Width = AlignSize(
            capacity * sizeof(T),
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        );
        GPUResource::CreateResource(name, pDevice, allocDesc, resDesc);
    }

    /**
     * @brief Returns the number of @p T elements that fit in the allocated buffer.
     * @return Element capacity (aligned allocation size / sizeof(T)).
     */
    size_t GetCapacity() const {
        return GPUResource::GetResourceDesc().Width / sizeof(T);
    }

    /** @brief Returns an SRV description treating the buffer as a structured buffer of @p T. */
    std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> GetSrvDesc() const override {
        return D3D12_SHADER_RESOURCE_VIEW_DESC{
            .ViewDimension{ D3D12_SRV_DIMENSION_BUFFER },
            .Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
            .Buffer{
                .NumElements{ static_cast<uint32_t>(GetCapacity()) },
                .StructureByteStride{ sizeof(T) }
            }
        };
    }

    /** @brief Returns a UAV description treating the buffer as a structured RW buffer of @p T. */
    std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> GetUavDesc() const override {
        return D3D12_UNORDERED_ACCESS_VIEW_DESC{
            .ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
            .Buffer{
                .NumElements{ static_cast<uint32_t>(GetCapacity()) },
                .StructureByteStride{ sizeof(T) }
            }
        };
    }

    /** @brief Returns a CBV description covering the full aligned buffer range. */
    std::optional<D3D12_CONSTANT_BUFFER_VIEW_DESC> GetCbvDesc() const override {
        return D3D12_CONSTANT_BUFFER_VIEW_DESC{
            .BufferLocation{ GetD3D12Resource()->GetGPUVirtualAddress() },
            .SizeInBytes{ AlignSize(
                GetCapacity() * sizeof(T),
                D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
            ) }
        };
    }

    /** @brief Returns an RTV description for the buffer (single-row, element count = capacity). */
    std::optional<D3D12_RENDER_TARGET_VIEW_DESC> GetRtvDesc() const override {
        return D3D12_RENDER_TARGET_VIEW_DESC{
            .ViewDimension{ D3D12_RTV_DIMENSION_BUFFER },
            .Buffer{
                .NumElements{ static_cast<uint32_t>(GetCapacity()) }
            }
        };
    }
};
