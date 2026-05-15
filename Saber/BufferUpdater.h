/**
 * @file BufferUpdater.h
 * @brief Pluggable GPU-upload strategies for @ref Buffer<T>.
 *
 * The hierarchy provides four concrete updaters:
 *  - @ref RangeBufferUpdater   — tracks the dirty [first, last] element range and
 *    uploads it in one copy (via intermediate resource or ring buffer).
 *  - @ref WholeBufferUpdater   — marks the whole buffer dirty on any write and
 *    uploads all storage data in one subresource update.
 *  - @ref DynamicBufferUpdater — accumulates sparse per-index writes and dispatches
 *    a GPU compute shader to scatter-write them into a UAV buffer.
 *  - @ref InstUploadBufferUpdater — writes directly to a persistently-mapped upload
 *    heap; no copy command needed.
 *
 * All concrete updaters are friends of @ref Buffer<T> (via
 * @ref RangeBufferUpdater and @ref WholeBufferUpdater) to access its protected
 * @c RecreateBufferAndViews helper.
 */
#pragma once

#include "Headers.h"

#include <utility>

#include "ComputeObject.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"

template <typename T>
class Buffer;

/**
 * @brief Abstract base for all GPU-upload strategies.
 *
 * @tparam T Element type of the owning @ref Buffer<T>.
 */
template <typename T>
class BufferUpdater {
protected:
	Buffer<T>& m_buffer; /**< @brief Reference to the owning buffer. */

public:
	/** @brief Constructs the updater bound to @p buffer. */
	BufferUpdater(Buffer<T>& buffer) : m_buffer(buffer) {}
	virtual ~BufferUpdater() = default;

	/**
	 * @brief Stages an update for all @p count elements.
	 * @param pData Pointer to source data array.
	 * @param count Number of elements.
	 */
	virtual void UpdateAll(const T* pData, size_t count) = 0;

	/**
	 * @brief Stages an update for the single element at @p id.
	 * @param id   Zero-based element index.
	 * @param data New element value.
	 */
	virtual void UpdateAt(size_t id, const T& data) = 0;

	/** @brief Returns @c true if there are staged writes not yet flushed to the GPU. */
	virtual bool IsUpdatePending() const = 0;

	/**
	 * @brief Flushes all staged writes to the GPU.
	 * @param pDeviceContext     Device context.
	 * @param pCommandListDirect Command list for copy/barrier commands.
	 */
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) = 0;
};

/**
 * @brief Concept satisfied by types that derive from @ref BufferUpdater<T>.
 * @tparam T       Element type.
 * @tparam Derived Candidate updater type.
 */
template <typename T, typename Derived>
concept BufferUpdaterConcept = std::derived_from<Derived, BufferUpdater<T>>;

/**
 * @brief Selects whether to allocate intermediate memory from a new GPU resource
 *        or from the per-frame @ref DynamicUploadHeap ring buffer.
 */
enum class BufferUpdaterMemoryType {
	Intermediate, /**< @brief Allocate a dedicated upload intermediate resource. */
	RingBuffer    /**< @brief Sub-allocate from the per-frame ring buffer. */
};

/**
 * @brief Uploads only the contiguous dirty element range [first, last].
 *
 * @c UpdateAll marks the range [0, count-1]; @c UpdateAt extends the range to
 * include @p id.  On @c PerformUpdate, the range is copied to the GPU in a
 * single @c CopyBufferRegion call using either an intermediate resource or a
 * ring-buffer allocation, then the range is reset to @c InvalidUpdRange.
 *
 * If the dirty range exceeds the GPU buffer capacity the buffer is recreated
 * at the new required size before the copy.
 *
 * @tparam T          Element type.
 * @tparam MemoryType Intermediate memory source.
 */
template <typename T, BufferUpdaterMemoryType MemoryType = BufferUpdaterMemoryType::Intermediate>
class RangeBufferUpdater : public BufferUpdater<T> {
	using UpdateRange = std::pair<size_t, size_t>;
	inline static constexpr UpdateRange InvalidUpdRange{
		static_cast<size_t>(-1), 0
	};
	UpdateRange m_updRange{ InvalidUpdRange };

public:
	/** @brief Constructs the updater bound to @p buffer. */
	RangeBufferUpdater(
		Buffer<T>& buffer
	) : BufferUpdater<T>(buffer) {}

	virtual void UpdateAll(const T* pData, size_t count) override {
		m_updRange = std::make_pair(0, count - 1);
	}
	virtual void UpdateAt(size_t id, const T& data) override {
		m_updRange.first = std::min(m_updRange.first, id);
		m_updRange.second = std::max(m_updRange.second, id);
	}

	virtual bool IsUpdatePending() const override {
		return m_updRange != InvalidUpdRange;
	}
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandList
	) override {
		if (!IsUpdatePending()) {
			return;
		}

		Buffer<T>& buffer{ m_buffer };
		size_t updCnt{ m_updRange.second - m_updRange.first + 1 };
		size_t updSize{ updCnt * sizeof(T) };

		D3D12_RESOURCE_STATES prevState{ buffer.GetResource()->GetState() };
		if (updCnt > buffer.GetCapacity()) {
			buffer.RecreateBufferAndViews(
				pDeviceContext->GetDevice(),
				updCnt,
				pCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_COPY
				? D3D12_RESOURCE_STATE_COMMON
				: D3D12_RESOURCE_STATE_COPY_DEST
			);
		}
		else if (pCommandList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			buffer.GetResource()->ResourceTransition(
				pCommandList,
				D3D12_RESOURCE_STATE_COPY_DEST
			);
		}

		if constexpr (MemoryType == BufferUpdaterMemoryType::Intermediate) {
			auto pIntermediate{ std::make_shared<GPUResource>(
				L"Intermediate",
				pDeviceContext->GetDevice(),
				GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_UPLOAD },
				GPUResource::ResourceDesc{
					CD3DX12_RESOURCE_DESC::Buffer(updSize),
					D3D12_RESOURCE_STATE_GENERIC_READ
				}
			) };

			T* pDst{};
			ThrowIfFailed(pIntermediate->GetD3D12Resource()->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pDst)));
			memcpy(pDst, &buffer.GetStorageData()[m_updRange.first], updSize);
			pIntermediate->GetD3D12Resource()->Unmap(0, &CD3DX12_RANGE(m_updRange.first, m_updRange.second));

			pCommandList->GetD3D12CommandList()->CopyBufferRegion(
				buffer.GetResource()->GetD3D12Resource().Get(),
				m_updRange.first * sizeof(T),
				pIntermediate->GetD3D12Resource().Get(),
				0,
				updSize
			);
			pDeviceContext->AddIntermediate(pIntermediate);
		}
		else {
			DynamicAllocation intermediateAllocation{
				pDeviceContext->GetRingBuffer()->Allocate(updSize)
			};
			memcpy(intermediateAllocation.cpuAddress, &buffer.GetStorageData()[m_updRange.first], updSize);
			pCommandList->GetD3D12CommandList()->CopyBufferRegion(
				buffer.GetResource()->GetD3D12Resource().Get(),
				m_updRange.first * sizeof(T),
				intermediateAllocation.pBuffer->GetD3D12Resource().Get(),
				intermediateAllocation.offset,
				updSize
			);
		}
		m_updRange = InvalidUpdRange;

		if (pCommandList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			buffer.GetResource()->ResourceTransition(
				pCommandList,
				prevState
			);
		}
	}
};

/**
 * @brief Marks the entire buffer dirty on any write and uploads all storage data at once.
 *
 * Any call to @c UpdateAll or @c UpdateAt sets an internal flag.  @c PerformUpdate
 * then copies all elements from CPU storage to the GPU using @c UpdateSubresources,
 * either via an intermediate resource or the ring buffer.
 *
 * @tparam T          Element type.
 * @tparam MemoryType Intermediate memory source.
 */
template <typename T, BufferUpdaterMemoryType MemoryType = BufferUpdaterMemoryType::Intermediate>
class WholeBufferUpdater : public BufferUpdater<T> {
	bool m_isUpdatePending{};

public:
	/** @brief Constructs the updater bound to @p buffer. */
	WholeBufferUpdater(
		Buffer<T>& buffer
	) : BufferUpdater<T>(buffer) {}

	virtual void UpdateAll(const T* pData, size_t count) override {
		m_isUpdatePending = true;
	}
	virtual void UpdateAt(size_t id, const T& data) override {
		m_isUpdatePending = true;
	}
	virtual bool IsUpdatePending() const override {
		return m_isUpdatePending;
	}
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandList
	) override {
		if (!IsUpdatePending()) {
			return;
		}
		m_isUpdatePending = false;

		Buffer<T>& buffer{ m_buffer };
		size_t updCnt{ buffer.GetStorageDataSize() };

		D3D12_RESOURCE_STATES prevState{ buffer.GetResource()->GetState() };
		if (updCnt > buffer.GetCapacity()) {
			buffer.RecreateBufferAndViews(
				pDeviceContext->GetDevice(),
				updCnt,
				pCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_COPY
				? D3D12_RESOURCE_STATE_COMMON
				: D3D12_RESOURCE_STATE_COPY_DEST
			);
		}
		else if (pCommandList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			buffer.GetResource()->ResourceTransition(
				pCommandList,
				D3D12_RESOURCE_STATE_COPY_DEST
			);
		}

		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ buffer.GetStorageData() },
			.RowPitch{ static_cast<UINT>(updCnt) * sizeof(T) },
			.SlicePitch{ subresData.RowPitch }
		};

		if constexpr (MemoryType == BufferUpdaterMemoryType::Intermediate) {
			std::shared_ptr<GPUResource> pIntermediate{
				buffer.GetResource()->CreateIntermediate(pDeviceContext->GetDevice())
			};
			buffer.GetResource()->UpdateSubresources(
				pCommandList,
				pIntermediate,
				&subresData
			);
			pDeviceContext->AddIntermediate(pIntermediate);
		}
		else {
			DynamicAllocation intermediateAllocation{
				pDeviceContext->GetRingBuffer()->Allocate(buffer.GetResource()->GetIntermediateSize())
			};
			buffer.GetResource()->UpdateSubresources(
				pCommandList,
				intermediateAllocation.pBuffer,
				&subresData,
				intermediateAllocation.offset
			);
		}

		if (pCommandList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			buffer.GetResource()->ResourceTransition(
				pCommandList,
				prevState
			);
		}
	}
};

/**
 * @brief Accumulates sparse per-element writes and flushes them via a GPU compute shader.
 *
 * On @c UpdateAt / @c UpdateAll, element indices and new values are appended to CPU
 * vectors.  @c PerformUpdate uploads those vectors to the ring buffer and dispatches
 * the bound @ref ComputeObject to scatter-write into the UAV buffer.  The buffer is
 * expanded automatically if the maximum written index exceeds the current capacity.
 *
 * @tparam T Element type (the buffer must have the UAV flag).
 */
template <typename T>
class DynamicBufferUpdater : public BufferUpdater<T> {
	std::shared_ptr<ComputeObject> m_pUpdater{}; /**< @brief Compute shader that performs the scatter-write. */

	std::vector<UINT> m_updBufIds{}; /**< @brief Indices of elements to update. */
	std::vector<T> m_updBuf{};       /**< @brief New values corresponding to @ref m_updBufIds. */
	size_t m_updMaxId{};             /**< @brief Highest index staged since last flush. */

public:
	/**
	 * @brief Constructs the updater.
	 * @param buffer   Owning UAV buffer.
	 * @param pUpdater Compute object implementing the scatter-write pass.
	 */
	DynamicBufferUpdater(
		Buffer<T>& buffer,
		std::shared_ptr<ComputeObject> pUpdater
	) : BufferUpdater<T>(buffer), m_pUpdater(pUpdater) {
		assert(m_buffer.GetResource()->IsUav());
		m_updBufIds.reserve(m_buffer.GetCapacity());
		m_updBuf.reserve(m_buffer.GetCapacity());
	}

	virtual void UpdateAll(const T* pData, size_t count) override {
		if (count > m_updBuf.capacity()) {
			m_updBufIds.reserve(count);
			m_updBuf.reserve(count);
			m_updMaxId = count - 1;
		}
		for (size_t i{}; i < count; ++i) {
			m_updBufIds.push_back(i);
			m_updBuf.push_back(pData[i]);
		}
	}
	virtual void UpdateAt(size_t id, const T& data) override {
		m_updBufIds.push_back(id);
		m_updBuf.push_back(data);
		m_updMaxId = id;
	}
	virtual bool IsUpdatePending() const override {
		return m_updBufIds.size();
	}
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) override {
		assert(m_updBufIds.size() == m_updBuf.size());
		size_t updCnt{ m_updBufIds.size() };
		if (!IsUpdatePending()) {
			return;
		}

		if (m_updMaxId + 1 > m_buffer.GetCapacity()) {
			m_buffer.Expand(
				pDeviceContext,
				pCommandListDirect,
				m_updMaxId + 1
			);
		}

		size_t updBufIdsSize{ updCnt * sizeof(UINT) };
		DynamicAllocation updBufIdsAllocation{ pDeviceContext->GetRingBuffer()->Allocate(updBufIdsSize) };
		memcpy(updBufIdsAllocation.cpuAddress, m_updBufIds.data(), updBufIdsSize);
		m_updBufIds.clear();

		size_t updBufSize{ updCnt * sizeof(T) };
		DynamicAllocation updBufAllocation{ pDeviceContext->GetRingBuffer()->Allocate(updBufSize) };
		memcpy(updBufAllocation.cpuAddress, m_updBuf.data(), updBufSize);
		m_updBuf.clear();

		static const size_t threadBlockSize{ 128 };
		m_buffer.GetResource()->ResourceTransition(pCommandListDirect, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_pUpdater->Dispatch(
			pCommandListDirect,
			{ static_cast<uint32_t>(updCnt + threadBlockSize - 1 / threadBlockSize), 1, 1 },
			[&](std::shared_ptr<CommandList> pCommandList, UINT& rootParamId) {
				auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };
				pD3D12CommandList->SetComputeRoot32BitConstant(rootParamId++, updCnt, 0);
				pD3D12CommandList->SetComputeRootShaderResourceView(rootParamId++, updBufIdsAllocation.gpuAddress);
				pD3D12CommandList->SetComputeRootShaderResourceView(rootParamId++, updBufAllocation.gpuAddress);
				pD3D12CommandList->SetComputeRootUnorderedAccessView(
					rootParamId++,
					m_buffer.GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
				);
			}
		);
	}

};

/**
 * @brief Writes directly to a persistently-mapped upload-heap buffer.
 *
 * @c UpdateAll / @c UpdateAt map the buffer, write the data, and unmap immediately.
 * There is no pending state — @c PerformUpdate is a no-op and @c IsUpdatePending
 * always returns @c false.
 *
 * @note The owning @ref Buffer<T> must be on @c D3D12_HEAP_TYPE_UPLOAD.
 *
 * @tparam T Element type.
 */
template <typename T>
class InstUploadBufferUpdater : public BufferUpdater<T> {
public:
	/** @brief Constructs the updater; asserts that the buffer is on the upload heap. */
	InstUploadBufferUpdater(
		Buffer<T>& buffer
	) : BufferUpdater<T>(buffer) {
		assert(m_buffer.GetResource()->GetHeapProperties().Type == D3D12_HEAP_TYPE_UPLOAD);
	}

	virtual void UpdateAll(const T* pData, size_t count) override {
		auto pD3D12Buffer{ m_buffer.GetResource()->GetD3D12Resource() };

		T* pDst{};
		ThrowIfFailed(pD3D12Buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pDst)));
		memcpy(pDst, pData, count * sizeof(T));
		pD3D12Buffer->Unmap(0, &CD3DX12_RANGE(0, count));
	}
	virtual void UpdateAt(size_t id, const T& data) override {
		auto pD3D12Buffer{ m_buffer.GetResource()->GetD3D12Resource() };

		T* pDst{};
		ThrowIfFailed(pD3D12Buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pDst)));
		pDst[id] = data;
		pD3D12Buffer->Unmap(0, &CD3DX12_RANGE(id, id + 1));
	}
	virtual bool IsUpdatePending() const override {
		return false;
	}
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) override {}
};
