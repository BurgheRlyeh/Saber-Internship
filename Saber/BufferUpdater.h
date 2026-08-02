#pragma once

#include "Headers.h"

#include <utility>

#include "CommandListTypes.h"
#include "ComputeObject.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"

template <typename T>
class Buffer;

template <typename T>
class BufferUpdater {
protected:
	Buffer<T>& m_buffer;

public:
	BufferUpdater(Buffer<T>& buffer) : m_buffer(buffer) {}
	virtual ~BufferUpdater() = default;

	virtual void UpdateAll(const T* pData, size_t count) = 0;
	virtual void UpdateAt(size_t id, const T& data) = 0;
	virtual bool IsUpdatePending() const = 0;
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) = 0;
};

template <typename T, typename Derived>
concept BufferUpdaterConcept = std::derived_from<Derived, BufferUpdater<T>>;

enum class BufferUpdaterMemoryType {
	Intermediate, RingBuffer
};

template <typename T, BufferUpdaterMemoryType MemoryType = BufferUpdaterMemoryType::Intermediate>
class RangeBufferUpdater : public BufferUpdater<T> {
	using UpdateRange = std::pair<size_t, size_t>;
	inline static constexpr UpdateRange InvalidUpdRange{
		static_cast<size_t>(-1), 0
	};
	UpdateRange m_updRange{ InvalidUpdRange };

public:
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
				pCommandList->GetType() == CommandListType::Copy
				? D3D12_RESOURCE_STATE_COMMON
				: D3D12_RESOURCE_STATE_COPY_DEST
			);
		}
		else if (pCommandList->GetType() != CommandListType::Copy) {
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

		if (pCommandList->GetType() != CommandListType::Copy) {
			buffer.GetResource()->ResourceTransition(
				pCommandList,
				prevState
			);
		}
	}
};

template <typename T, BufferUpdaterMemoryType MemoryType = BufferUpdaterMemoryType::Intermediate>
class WholeBufferUpdater : public BufferUpdater<T> {
	bool m_isUpdatePending{};

public:
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
				pCommandList->GetType() == CommandListType::Copy
				? D3D12_RESOURCE_STATE_COMMON
				: D3D12_RESOURCE_STATE_COPY_DEST
			);
		}
		else if (pCommandList->GetType() != CommandListType::Copy) {
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

		if (pCommandList->GetType() != CommandListType::Copy) {
			buffer.GetResource()->ResourceTransition(
				pCommandList,
				prevState
			);
		}
	}
};

template <typename T>
class DynamicBufferUpdater : public BufferUpdater<T> {
	std::shared_ptr<ComputeObject> m_pUpdater{};

	std::vector<UINT> m_updBufIds{};
	std::vector<T> m_updBuf{};
	size_t m_updMaxId{};

public:
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

template <typename T>
class InstUploadBufferUpdater : public BufferUpdater<T> {
public:
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
