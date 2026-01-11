#pragma once

#include "Headers.h"

#include "GPUResource.h"

template <typename T>
class BufferResource : public GPUResource {
	size_t m_capacity{};

public:
	BufferResource(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		size_t capacity = 1,
		const AllocationDesc& allocDesc = {},
		ResourceDesc& resDesc = { CD3DX12_RESOURCE_DESC::Buffer(0) }
	) {
		CreateResource(name, pDevice, capacity, allocDesc, resDesc);
	}

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
		m_capacity = capacity;
		resDesc.resDesc.Width = m_capacity * sizeof(T);
		GPUResource::CreateResource(name, pDevice, allocDesc, resDesc);
	}

	size_t GetCapacity() const {
		return m_capacity;
	}

	std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> GetSrvDesc() const override {
		return D3D12_SHADER_RESOURCE_VIEW_DESC{
			.ViewDimension{ D3D12_SRV_DIMENSION_BUFFER },
			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			.Buffer{
				.NumElements{ static_cast<uint32_t>(m_capacity) },
				.StructureByteStride{ sizeof(T) }
			}
		};
	}
	std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> GetUavDesc() const override {
		return D3D12_UNORDERED_ACCESS_VIEW_DESC{
			.ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
			.Buffer{
				.NumElements{ static_cast<uint32_t>(m_capacity) },
				.StructureByteStride{ sizeof(T) }
			}
		};
	}
	std::optional<D3D12_CONSTANT_BUFFER_VIEW_DESC> GetCbvDesc() const override {
		return D3D12_CONSTANT_BUFFER_VIEW_DESC{
			.BufferLocation{ GetD3D12Resource()->GetGPUVirtualAddress() },
			.SizeInBytes{ AlignSize(
				m_capacity * sizeof(T),
				D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
			) }
		};
	}
	std::optional<D3D12_RENDER_TARGET_VIEW_DESC> GetRtvDesc() const override {
		return D3D12_RENDER_TARGET_VIEW_DESC{
			.ViewDimension{ D3D12_RTV_DIMENSION_BUFFER },
			.Buffer{
				.NumElements{ static_cast<uint32_t>(m_capacity) }
			}
		};
	}
};