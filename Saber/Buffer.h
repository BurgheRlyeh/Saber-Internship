#pragma once

#include "Headers.h"

#include <array>
#include <bit>

#include "BufferResource.h"
#include "BufferUpdater.h"
#include "ComputeObject.h"
#include "DeviceContext.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"

template <typename T>
class Buffer {
	friend class RangeBufferUpdater<T>;
	friend class WholeBufferUpdater<T>;

protected:
	std::wstring m_name{};

	std::shared_ptr<BufferResource<T>> m_pResource{};

	std::array<std::shared_ptr<DescRange>, static_cast<size_t>(DescRangeType::ResNumTypes)> m_pDescHeapRanges{};

	std::vector<T> m_data{};

	std::unique_ptr<BufferUpdater<T>> m_pUpdater{};

	D3D12MA::ALLOCATION_FLAGS m_allocationFlags{};

public:
	Buffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		size_t capacity,
		const GPUResource::AllocationDesc& allocDesc = {},
		const GPUResource::ResourceDesc& resDesc = { CD3DX12_RESOURCE_DESC::Buffer(0) },
		const EnumFlags<ResourceView> views = ResourceView::Any
	) : m_name(name) {
		for (size_t rangeTypeId{}; rangeTypeId < static_cast<size_t>(DescRangeType::ResNumTypes); ++rangeTypeId) {
			ResourceView view{ FromId<ResourceView>(rangeTypeId) };
			if ((view & views) && SupportsView(view, resDesc.resDesc)) {
				m_pDescHeapRanges[rangeTypeId] = pDeviceContext->GetDescriptorHeap()->AllocateRange(
					m_name + L"/Ranges/" + ToName(FromId<DescRangeType>(rangeTypeId)), 1
				);
			}
		}

		GPUResource::ResourceDesc resDescCopy{ resDesc };
		CreateBufferAndViews(
			pDeviceContext->GetDevice(),
			capacity,
			allocDesc,
			resDescCopy
		);
	}

	std::shared_ptr<DescRange> GetDescRange(DescRangeType type) const {
		return m_pDescHeapRanges[ToId(type)];
	}

	virtual ~Buffer() = default;

	std::shared_ptr<BufferResource<T>> GetResource() const {
		return m_pResource;
	}

	size_t GetCapacity() const {
		return GetResource()->GetCapacity();
	}

	template<typename Updater, typename... Args>
	requires BufferUpdaterConcept<T, Updater>
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
		if (numElements <= GetCapacity()) {
			return false;
		}

		std::shared_ptr<GPUResource> pOldResource{ m_pResource };
		uint32_t oldCapacity{ static_cast<uint32_t>(GetCapacity()) };
		D3D12_RESOURCE_STATES oldState{ pOldResource->GetState() };

		RecreateBufferAndViews(
			pDeviceContext->GetDevice(),
			numElements,
			pCommandListDirect->GetType() == D3D12_COMMAND_LIST_TYPE_COPY
			? D3D12_RESOURCE_STATE_COMMON
			: D3D12_RESOURCE_STATE_COPY_DEST
		);

		if (pCommandListDirect->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			pOldResource->ResourceTransition(
				pCommandListDirect,
				D3D12_RESOURCE_STATE_COPY_SOURCE
			);
		}

		pCommandListDirect->GetD3D12CommandList()->CopyBufferRegion(
			m_pResource->GetD3D12Resource().Get(),
			0,
			pOldResource->GetD3D12Resource().Get(),
			0,
			oldCapacity * sizeof(T)
		);
		pDeviceContext->AddIntermediate(pOldResource);

		if (pCommandListDirect->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			m_pResource->ResourceTransition(
				pCommandListDirect,
				oldState
			);
		}

		return true;
	}

protected:
	void CreateBufferAndViews(
		std::shared_ptr<Device> pDevice,
		uint32_t numElements,
		const GPUResource::AllocationDesc& allocDesc,
		GPUResource::ResourceDesc& resDesc
	) {
		resDesc.resDesc.Width = numElements * sizeof(T);
		m_pResource = std::make_shared<BufferResource<T>>(
			m_name,
			pDevice,
			numElements,
			allocDesc,
			resDesc
		);

		for (size_t rangeTypeId{}; rangeTypeId < static_cast<size_t>(DescRangeType::ResNumTypes); ++rangeTypeId) {
			if (auto& pRange = m_pDescHeapRanges[rangeTypeId]) {
				pRange->Clear();
				m_pResource->CreateResourceView(
					FromId<ResourceView>(rangeTypeId),
					pDevice,
					pRange->GetNextCpuHandle()
				);
			}
		}
	}

	void RecreateBufferAndViews(
		std::shared_ptr<Device> pDevice,
		uint32_t numElements,
		D3D12_RESOURCE_STATES initState
	) {
		assert(GetResource());

		GPUResource::AllocationDesc allocDesc{
			GetResource()->GetHeapProperties().Type,
			GetResource()->GetHeapFlags(),
			m_allocationFlags
		};
		GPUResource::ResourceDesc resDesc{
			GetResource()->GetResourceDesc(),
			initState
		};
		CreateBufferAndViews(
			pDevice,
			numElements,
			allocDesc,
			resDesc
		);
	}
};

template <
	typename T,
	template <typename> typename UploadBufferUpdater = InstUploadBufferUpdater
> requires BufferUpdaterConcept<T, UploadBufferUpdater<T>>
static std::shared_ptr<Buffer<T>> CreateUploadBufferWithUpdater(
	const std::wstring& name,
	std::shared_ptr<DeviceContext> pDeviceContext,
	const EnumFlags<ResourceView> views = ResourceView::None
) {
	auto pBuffer{ std::make_shared<Buffer<T>>(
		name,
		pDeviceContext,
		1,
		GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_UPLOAD },
		GPUResource::ResourceDesc{
			CD3DX12_RESOURCE_DESC::Buffer(0),
			D3D12_RESOURCE_STATE_GENERIC_READ
		},
		views
	) };
	pBuffer->CreateUpdater<UploadBufferUpdater<T>>();
	return pBuffer;
}
