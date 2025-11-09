#pragma once

#include "Headers.h"

#include <bit>
#include <climits>

#include "Buffer.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "GPUResource.h"

//class VisibilityBuffer : public Buffer<DirectX::XMUINT4> {
//	static constexpr size_t BITS_PER_UINT4{ 4 * sizeof(uint32_t) * CHAR_BIT };
//	static constexpr size_t LOG2_BITS_PER_UINT4{ std::bit_width(BITS_PER_UINT4) - 1 };
//
//public:
//	VisibilityBuffer(
//		const std::wstring& name,
//		std::shared_ptr<DeviceContext> pDeviceContext,
//		size_t capacity,
//		const GPUResource::HeapData& heapData = GPUResource::HeapData{},
//		const GPUResource::ResourceData& resData = GPUResource::ResourceData{ CD3DX12_RESOURCE_DESC::Buffer(0) },
//		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
//	) : Buffer< DirectX::XMUINT4>(
//		name,
//		pDeviceContext,
//		ElementsToCapacity(capacity),
//		heapData,
//		resData,
//		allocationFlags
//	) {}
//
//	bool Expand(
//		std::shared_ptr<DeviceContext> pDeviceContext,
//		std::shared_ptr<CommandQueue> pCommandQueueDirect,
//		uint32_t numElements
//	) override {
//		Buffer::Expand(pDeviceContext, pCommandQueueDirect, ElementsToCapacity(numElements));
//	}
//
//protected:
//	void CreateBuffersAndViews(
//		std::shared_ptr<DeviceContext> pDeviceContext,
//		uint32_t numElements
//	) override {
//		Buffer::CreateBuffersAndViews(pDeviceContext, ElementsToCapacity(numElements));
//	}
//
//private:
//	static size_t ElementsToCapacity(size_t numElements) {
//		numElements = std::max<size_t>(BITS_PER_UINT4, std::bit_ceil(numElements));
//		return numElements >> LOG2_BITS_PER_UINT4;
//	}
//
//	static size_t CapacityToElements(size_t numElements) {
//		return numElements << LOG2_BITS_PER_UINT4;
//	}
//};

class VisibilityBuffer {
	static constexpr size_t BITS_PER_UINT4{ 4 * sizeof(uint32_t) * CHAR_BIT };
	static constexpr size_t LOG2_BITS_PER_UINT4{ std::bit_width(BITS_PER_UINT4) - 1 };

	std::wstring m_name{};

	std::shared_ptr<GPUResource> m_pVisibilityBuffer{};
	std::shared_ptr<DescHeapRange> m_pDescHeapRangeUav{};

	size_t m_capacity{};

public:
	VisibilityBuffer(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		size_t& numElements
	) : m_name(name + L"/VisibilityBuffer") {
		m_pDescHeapRangeUav = pDescHeapManagerCbvSrvUav->AllocateRange(
			(m_name + L"/Ranges/Uav").c_str(),
			1,
			D3D12_DESCRIPTOR_RANGE_TYPE_UAV
		);

		m_capacity = ElementsToCapacity(numElements);
		numElements = CapacityToElements(m_capacity);

		m_pVisibilityBuffer = std::make_shared<GPUResource>(
			m_name,
			pDevice,
			GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
			GPUResource::ResourceData{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					m_capacity,
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ D3D12_RESOURCE_STATE_COPY_DEST }
			}
		);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
			//.Format{ DXGI_FORMAT_R32_TYPELESS },	// ?
			.ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
			.Buffer{
				.NumElements{ m_capacity },
				.StructureByteStride{ 4 * sizeof(uint32_t) },
				//.Flags{ D3D12_BUFFER_UAV_FLAG_RAW }	// ?
			}
		};
		m_pVisibilityBuffer->CreateUnorderedAccessView(
			pDevice,
			m_pDescHeapRangeUav->GetNextCpuHandle(),
			&uavDesc
		);

		std::vector<uint32_t> nulls(m_capacity, 0);
		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ nulls.data() },
			.RowPitch{ static_cast<UINT>(nulls.size()) * sizeof(uint32_t) },
			.SlicePitch{ subresData.RowPitch }
		};

		std::shared_ptr<GPUResource> pIntermediate{ m_pVisibilityBuffer->CreateIntermediate(pDevice) };
		std::shared_ptr<CommandList> pCommandListCopy{
			pCommandQueueCopy->GetCommandList(pDevice)
		};
		m_pVisibilityBuffer->UpdateSubresources(
			pCommandListCopy,
			pIntermediate,
			&subresData
		);
		pCommandQueueCopy->ExecuteCommandListImmediately(pCommandListCopy);

		std::shared_ptr<CommandList> pCommandListDirect{ pCommandQueueDirect->GetCommandList(pDevice) };
		m_pVisibilityBuffer->ResourceTransition(
			pCommandListDirect,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	}

private:
	static size_t ElementsToCapacity(size_t numElements) {
		numElements = std::max<size_t>(BITS_PER_UINT4, std::bit_ceil(numElements));
		return numElements >> LOG2_BITS_PER_UINT4;
	}

	static size_t CapacityToElements(size_t numElements) {
		return numElements << LOG2_BITS_PER_UINT4;
	}
};
