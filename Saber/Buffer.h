#pragma once

#include "Headers.h"

#include "BufferUpdater.h"
#include "ComputeObject.h"
#include "DeviceContext.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"

template <typename T>
class Buffer {
	friend class StaticBufferUpdater<T>;

protected:
	std::wstring m_name{};

	std::shared_ptr<GPUResource> m_pResource{};

	std::shared_ptr<DescHeapRange> m_pSrvsRange{};
	std::shared_ptr<DescHeapRange> m_pRtvsRange{};
	std::shared_ptr<DescHeapRange> m_pUavsRange{};

	size_t m_capacity{};
	std::vector<T> m_data{};

	std::unique_ptr<BufferUpdater<T>> m_pUpdater{};

	GPUResource::HeapData m_heapData{};
	GPUResource::ResourceData m_resData{};
	D3D12MA::ALLOCATION_FLAGS m_allocFlags{};

public:
	Buffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		size_t capacity,
		const GPUResource::HeapData& heapData = GPUResource::HeapData{},
		const GPUResource::ResourceData& resData = GPUResource::ResourceData{ CD3DX12_RESOURCE_DESC::Buffer(0) },
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	) : m_name(name),
		m_capacity(capacity),
		m_heapData(heapData),
		m_resData(resData),
		m_allocFlags(allocationFlags)
	{
		
		if (IsSrvDesc(resData.resDesc)) {
			m_pSrvsRange = pDeviceContext->GetDescriptorHeap()->AllocateRange(
				m_name + L"/Ranges/Srv",
				1,
				D3D12_DESCRIPTOR_RANGE_TYPE_SRV
			);
		}
		if (IsUavDesc(resData.resDesc)) {
			m_pUavsRange = pDeviceContext->GetDescriptorHeap()->AllocateRange(
				m_name + L"/Ranges/Uav",
				1,
				D3D12_DESCRIPTOR_RANGE_TYPE_UAV
			);
		}
		if (IsRtvDesc(resData.resDesc)) {
			m_pRtvsRange = pDeviceContext->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->AllocateRange(
				m_name + L"/Ranges/Rtv",
				1
			);
		}

		CreateBuffersAndViews(pDeviceContext->GetDevice(), capacity);
	}

	virtual ~Buffer() = default;

	std::shared_ptr<GPUResource> GetResource() const {
		return m_pResource;
	}

	size_t GetCapacity() const {
		return m_capacity;
	}

	template<std::derived_from<BufferUpdater<T>> Updater, typename... Args>
	void CreateUpdater(Args&&... args) {
		m_pUpdater = std::make_unique<Updater>(*this, std::forward<Args>(args)...);
	}

	void SetUpdateAll(T* pData, size_t count) {
		assert(m_pUpdater);
		m_pUpdater->SetUpdateAll(pData, count);
	}
	void SetUpdateAt(size_t id, const T& data) {
		assert(m_pUpdater);
		m_pUpdater->SetUpdateAt(id, data);
	}
	void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) {
		assert(m_pUpdater);
		m_pUpdater->PerformUpdate(
			pDeviceContext,
			pCommandListDirect
		);
	}

	bool IsUpdatePending() const {
		return m_pUpdater ? m_pUpdater->IsUpdatePending() : false;
	}

	bool Expand(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect,
		uint32_t numElements
	) {
		if (numElements <= m_capacity) {
			return false;
		}

		std::shared_ptr<GPUResource> pOldResource{ m_pResource };
		uint32_t oldCapacity{ static_cast<uint32_t>(m_capacity) };

		CreateBuffersAndViews(pDeviceContext->GetDevice(), numElements);

		if (pCommandListDirect->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			pOldResource->ResourceTransition(
				pCommandListDirect,
				D3D12_RESOURCE_STATE_COPY_SOURCE
			);
			m_pResource->ResourceTransition(
				pCommandListDirect,
				D3D12_RESOURCE_STATE_COPY_DEST
			);
		}

		pCommandListDirect->GetD3D12CommandList()->CopyBufferRegion(
			m_pResource->GetResource().Get(),
			0,
			pOldResource->GetResource().Get(),
			0,
			oldCapacity * sizeof(T)
		);
		pDeviceContext->AddIntermediate(pOldResource);

		if (pCommandListDirect->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			m_pResource->ResourceTransition(
				pCommandListDirect,
				m_resData.resInitState
			);
		}

		return true;
	}

protected:
	void CreateBuffersAndViews(
		std::shared_ptr<Device> pDevice,
		uint32_t numElements
	) {
		m_capacity = numElements;
		
		m_resData.resDesc.Width = m_capacity * sizeof(T);
		m_pResource = std::make_shared<GPUResource>(
			m_name,
			pDevice,
			m_heapData,
			m_resData,
			m_allocFlags
		);

		if (m_pSrvsRange) {
			m_pSrvsRange->Clear();
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
				.ViewDimension{ D3D12_SRV_DIMENSION_BUFFER },
				.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
				.Buffer{
					.NumElements{ static_cast<uint32_t>(m_capacity) },
					.StructureByteStride{ sizeof(T) }
				}
			};
			m_pResource->CreateShaderResourceView(
				pDevice,
				m_pSrvsRange->GetNextCpuHandle(),
				&srvDesc
			);
		}

		if (m_pUavsRange) {
			m_pUavsRange->Clear();
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
				.ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
				.Buffer{
					.NumElements{ static_cast<uint32_t>(m_capacity) },
					.StructureByteStride{ sizeof(T) }
				}
			};
			m_pResource->CreateUnorderedAccessView(
				pDevice,
				m_pUavsRange->GetNextCpuHandle(),
				&uavDesc
			);
		}

		if (m_pRtvsRange) {
			m_pRtvsRange->Clear();
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
				.ViewDimension{ D3D12_RTV_DIMENSION_BUFFER },
				.Buffer{
					.NumElements{ static_cast<uint32_t>(m_capacity) }
				}
			};
			m_pResource->CreateRenderTargetView(
				pDevice,
				m_pRtvsRange->GetNextCpuHandle(),
				&rtvDesc
			);
		}
	}
};
