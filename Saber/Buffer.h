/**
 * @file Buffer.h
 * @brief Generic GPU buffer template wrapping @ref BufferResource, optional CPU-side storage,
 *        and a pluggable updater strategy.
 *
 * A @ref Buffer<T> owns:
 *  - A @ref BufferResource<T> (the D3D12 GPU resource).
 *  - One descriptor-heap range per supported view type (SRV, UAV, CBV, …).
 *  - An optional @ref BufferStorage<T> for CPU-side mirroring.
 *  - An optional @ref BufferUpdater<T> that controls how CPU data is flushed to the GPU.
 *
 * The free function @ref CreateUploadBufferWithUpdater provides a convenience factory for
 * upload-heap buffers backed by an @ref InstUploadBufferUpdater.
 */
#pragma once

#include "Headers.h"

#include <array>
#include <bit>

#include "BufferResource.h"
#include "BufferStorage.h"
#include "BufferUpdater.h"
#include "ComputeObject.h"
#include "DeviceContext.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"

/**
 * @brief Generic GPU buffer of elements of type @p T.
 *
 * @tparam T Element type stored in the buffer.
 */
template <typename T>
class Buffer {
	friend class RangeBufferUpdater<T>;
	friend class WholeBufferUpdater<T>;

protected:
	std::wstring m_name{};

	std::shared_ptr<BufferResource<T>> m_pResource{};

	/** @brief One descriptor range per view type; null for unsupported view types. */
	std::array<std::shared_ptr<DescRange>, static_cast<size_t>(DescRangeType::ResNumTypes)> m_pDescHeapRanges{};

	std::unique_ptr<BufferStorage<T>> m_pStorage{};  /**< @brief Optional CPU-side mirror. */
	std::unique_ptr<BufferUpdater<T>> m_pUpdater{};  /**< @brief Pluggable GPU-upload strategy. */

	D3D12MA::ALLOCATION_FLAGS m_allocationFlags{};

public:
	/**
	 * @brief Constructs the buffer, allocates the GPU resource, and creates descriptor ranges.
	 * @param name           Debug name.
	 * @param pDeviceContext Device context providing the device and descriptor heaps.
	 * @param capacity       Initial element capacity.
	 * @param allocDesc      D3D12MA allocation descriptor (heap type, flags, etc.).
	 * @param resDesc        D3D12 resource descriptor and initial state.
	 * @param views          Bitmask of @ref ResourceView flags; only supported views are allocated.
	 */
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
	virtual ~Buffer() = default;

	/** @brief Returns the underlying GPU buffer resource. */
	std::shared_ptr<BufferResource<T>> GetResource() const {
		return m_pResource;
	}

	/**
	 * @brief Returns the descriptor range for the given view type.
	 * @param type View type to look up.
	 * @return Descriptor range, or @c nullptr if the view is not supported.
	 */
	std::shared_ptr<DescRange> GetDescRange(DescRangeType type) const {
		return m_pDescHeapRanges[ToId(type)];
	}

	/** @brief Returns the element capacity of the GPU resource. */
	size_t GetCapacity() const {
		return GetResource()->GetCapacity();
	}

	/**
	 * @brief Creates and attaches a @ref BufferStorage of type @p Storage.
	 * @tparam Storage Storage type; must satisfy @ref BufferStorageConcept<T>.
	 * @tparam Args    Constructor arguments forwarded to the storage.
	 */
	template<typename Storage, typename... Args>
	requires BufferStorageConcept<T, Storage>
	void CreateStorage(Args&&... args) {
		m_pStorage = std::make_unique<Storage>(*this, std::forward<Args>(args)...);
	}

	/**
	 * @brief Creates and attaches a @ref BufferUpdater of type @p Updater.
	 * @tparam Updater Updater type; must satisfy @ref BufferUpdaterConcept<T>.
	 * @tparam Args    Constructor arguments forwarded to the updater.
	 */
	template<typename Updater, typename... Args>
	requires BufferUpdaterConcept<T, Updater>
	void CreateUpdater(Args&&... args) {
		m_pUpdater = std::make_unique<Updater>(*this, std::forward<Args>(args)...);
	}

	/** @brief Returns a pointer to the CPU-side storage data, or @c nullptr if no storage exists. */
	T* GetStorageData() {
		return m_pStorage ? m_pStorage->GetData() : nullptr;
	}
	/** @brief Returns the element count in the CPU-side storage, or 0 if no storage exists. */
	size_t GetStorageDataSize() const {
		return m_pStorage ? m_pStorage->GetDataSize() : 0;
	}

	/**
	 * @brief Stages an update for all @p count elements and mirrors them in storage if present.
	 * @param pData Pointer to the source data.
	 * @param count Number of elements to update.
	 */
	void UpdateAll(const T* pData, size_t count) {
		assert(m_pUpdater);
		if (m_pStorage) {
			m_pStorage->UpdateAll(pData, count);
		}
		m_pUpdater->UpdateAll(pData, count);
	}
	/**
	 * @brief Stages an update for the single element at @p id.
	 * @param id   Zero-based element index.
	 * @param data New element value.
	 */
	void UpdateAt(size_t id, const T& data) {
		assert(m_pUpdater);
		if (m_pStorage) {
			m_pStorage->UpdateAt(id, data);
		}
		m_pUpdater->UpdateAt(id, data);
	}
	/**
	 * @brief Flushes all pending staged updates to the GPU.
	 * @param pDeviceContext      Device context for resource management.
	 * @param pCommandListDirect  Command list used for copy/transition commands.
	 */
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

	/** @brief Returns @c true if there is a staged update waiting to be flushed. */
	bool IsUpdatePending() const {
		return m_pUpdater ? m_pUpdater->IsUpdatePending() : false;
	}

	/**
	 * @brief Grows the GPU buffer to @p numElements if it is currently smaller.
	 *
	 * Copies existing contents to the new resource via a GPU copy command and keeps
	 * the old resource alive in the intermediate list until the GPU is done with it.
	 *
	 * @param pDeviceContext     Device context.
	 * @param pCommandListDirect Command list for the copy.
	 * @param numElements        Requested new capacity.
	 * @return @c true if the buffer was expanded; @c false if already large enough.
	 */
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
	/**
	 * @brief Creates (or recreates) the GPU resource and all enabled descriptor views.
	 * @param pDevice     D3D12 device wrapper.
	 * @param numElements Element capacity for the new resource.
	 * @param allocDesc   Allocation descriptor.
	 * @param resDesc     Resource descriptor (modified in place to set the buffer byte size).
	 */
	void CreateBufferAndViews(
		std::shared_ptr<Device> pDevice,
		uint32_t numElements,
		const GPUResource::AllocationDesc& allocDesc,
		GPUResource::ResourceDesc& resDesc
	) {
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

	/**
	 * @brief Recreates the GPU buffer at a new capacity, preserving heap properties.
	 * @param pDevice    D3D12 device wrapper.
	 * @param numElements New element capacity.
	 * @param initState  Initial resource state for the new resource.
	 */
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

/**
 * @brief Creates an upload-heap @ref Buffer<T> pre-configured with an @p UploadBufferUpdater.
 *
 * The buffer is allocated on @c D3D12_HEAP_TYPE_UPLOAD with capacity 1.  The caller should
 * expand it as needed via @ref Buffer::Expand.
 *
 * @tparam T                  Element type.
 * @tparam UploadBufferUpdater Updater template; must produce a type satisfying
 *                            @ref BufferUpdaterConcept<T> (default @ref InstUploadBufferUpdater).
 * @param name           Debug name for the buffer.
 * @param pDeviceContext Device context.
 * @param views          Descriptor view flags (default @ref ResourceView::None).
 * @return Shared pointer to the created buffer.
 */
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
