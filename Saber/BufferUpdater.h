#pragma once

#include "Headers.h"

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

	virtual void SetUpdateAll(T* pData, size_t count) = 0;
	virtual void SetUpdateAt(size_t id, const T& data) = 0;
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) = 0;

	virtual bool IsUpdatePending() const = 0;
};

template <typename T, typename Derived>
concept BufferUpdaterConcept = std::derived_from<Derived, BufferUpdater<T>>;

template <typename T>
class StaticBufferUpdater : public BufferUpdater<T> {
	bool m_isUpdatePending{};

public:
	StaticBufferUpdater(
		Buffer<T>& buffer
	) : BufferUpdater<T>(buffer) {}

	virtual void SetUpdateAll(T* pData, size_t count) override {
		if (count > m_buffer.m_data.size()) {
			m_buffer.m_data.resize(count);
		}
		for (size_t i{}; i < count; ++i) {
			m_buffer.m_data[i] = pData[i];
		}
		m_isUpdatePending = true;
	}
	virtual void SetUpdateAt(size_t id, const T& data) override {
		if (id >= m_buffer.m_data.size()) {
			m_buffer.m_data.resize(id + 1);
		}
		m_buffer.m_data[id] = data;
		m_isUpdatePending = true;
	}
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandList
	) override {
		if (!m_isUpdatePending) {
			return;
		}
		m_isUpdatePending = false;

		Buffer<T>& buffer{ m_buffer };
		size_t updCnt{ buffer.m_data.size() };

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

		DynamicAllocation intermediateAllocation{
			pDeviceContext->GetRingBuffer()->Allocate(updCnt * sizeof(T))
		};
		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ buffer.m_data.data() },
			.RowPitch{ static_cast<UINT>(updCnt) * sizeof(T) },
			.SlicePitch{ subresData.RowPitch }
		};

		buffer.GetResource()->UpdateSubresources(
			pCommandList,
			intermediateAllocation.pBuffer,
			&subresData,
			intermediateAllocation.offset
		);

		if (pCommandList->GetType() != D3D12_COMMAND_LIST_TYPE_COPY) {
			buffer.GetResource()->ResourceTransition(
				pCommandList,
				prevState
			);
		}
	}

	virtual bool IsUpdatePending() const override {
		return m_isUpdatePending;
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

	virtual void SetUpdateAll(T* pData, size_t count) override {
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
	virtual void SetUpdateAt(size_t id, const T& data) override {
		m_updBufIds.push_back(id);
		m_updBuf.push_back(data);
		m_updMaxId = id;
	}
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) override {
		assert(m_updBufIds.size() == m_updBuf.size());
		size_t updCnt{ m_updBufIds.size() };
		if (!updCnt) {
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

	virtual bool IsUpdatePending() const override {
		return m_updBufIds.size();
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

	virtual void SetUpdateAll(T* pData, size_t count) override {
		auto pD3D12Buffer{ m_buffer.GetResource()->GetD3D12Resource() };

		T* pDst{};
		ThrowIfFailed(pD3D12Buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pDst)));
		memcpy(pDst, pData, count * sizeof(T));
		pD3D12Buffer->Unmap(0, &CD3DX12_RANGE(0, count));
	}
	virtual void SetUpdateAt(size_t id, const T& data) override {
		auto pD3D12Buffer{ m_buffer.GetResource()->GetD3D12Resource() };

		T* pDst{};
		ThrowIfFailed(pD3D12Buffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pDst)));
		pDst[id] = data;
		pD3D12Buffer->Unmap(0, &CD3DX12_RANGE(id, id + 1));
	}
	virtual void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	) override {}

	virtual bool IsUpdatePending() const override {
		return false;
	}
};
