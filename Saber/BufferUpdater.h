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
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) = 0;
};

template <typename T>
class StaticBufferUpdater : public BufferUpdater<T> {
	std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeap{};

public:
	StaticBufferUpdater(
		Buffer<T>& buffer,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap
	) : BufferUpdater<T>(buffer),
		m_pDynamicUploadHeap(pDynamicUploadHeap)
	{}

	virtual void SetUpdateAll(T* pData, size_t count) override {
		if (count > m_buffer.m_capacity) {
			m_buffer.m_data.resize(count);
		}
		for (size_t i{}; i < count; ++i) {
			m_buffer.m_data[i] = pData[i];
		}
	}
	virtual void SetUpdateAt(size_t id, const T& data) override {
		if (id >= m_buffer.m_data.size()) {
			m_buffer.m_data.resize(id + 1);
		}
		m_buffer.m_data[id] = data;
	}
	virtual void PerformUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) override {
		if (m_buffer.m_data.size() > m_buffer.m_capacity) {
			m_buffer.CreateBuffersAndViews(pDevice, pAllocator, m_buffer.m_data.size());
		}

		std::shared_ptr<CommandList> pCommandListDirect{
			pCommandQueueDirect->GetCommandList(pDevice)
		};
		m_buffer.m_pResource->ResourceTransition(
			pCommandListDirect,
			D3D12_RESOURCE_STATE_COPY_DEST
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);

		DynamicAllocation intermediateAllocation{
			m_pDynamicUploadHeap->Allocate(m_buffer.m_capacity * sizeof(T))
		};
		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ m_buffer.m_data.data() },
			.RowPitch{ static_cast<UINT>(m_buffer.m_data.size()) * sizeof(T) },
			.SlicePitch{ subresData.RowPitch }
		};

		std::shared_ptr<CommandList> pCommandListCopy{
			pCommandQueueCopy->GetCommandList(pDevice)
		};
		UpdateSubresources(
			pCommandListCopy->GetD3D12CommandList().Get(),
			m_buffer.m_pResource->GetResource().Get(),
			intermediateAllocation.pBuffer->GetResource().Get(),
			intermediateAllocation.offset,
			0,
			1,
			&subresData
		);
		pCommandQueueCopy->ExecuteCommandListImmediately(pCommandListCopy);

		pCommandListDirect = pCommandQueueDirect->GetCommandList(pDevice);
		m_buffer.m_pResource->ResourceTransition(
			pCommandListDirect,
			m_buffer.m_resData.resInitState
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	}
};

template <typename T>
class DynamicBufferUpdater : public BufferUpdater<T> {
	std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeap{};
	std::shared_ptr<ComputeObject> m_pUpdater{};

	std::vector<UINT> m_updBufIds{};
	std::vector<T> m_updBuf{};
	size_t m_updMaxId{};

public:
	DynamicBufferUpdater(
		Buffer<T>& buffer,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
		std::shared_ptr<ComputeObject> pUpdater
	) : BufferUpdater<T>(buffer),
		m_pDynamicUploadHeap(pDynamicUploadHeap),
		m_pUpdater(pUpdater)
	{
		assert(m_buffer.m_pResource->IsUav());
		m_updBufIds.reserve(m_buffer.m_capacity);
		m_updBuf.reserve(m_buffer.m_capacity);
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
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) override {
		assert(m_updBufIds.size() == m_updBuf.size());
		size_t updCnt{ m_updBufIds.size() };
		if (!updCnt) {
			return;
		}

		if (m_updMaxId + 1 > m_buffer.m_capacity) {
			m_buffer.Expand(
				pDevice,
				pAllocator,
				pCommandQueueCopy,
				pCommandQueueDirect,
				m_updMaxId + 1
			);
		}

		size_t updBufIdsSize{ updCnt * sizeof(UINT) };
		DynamicAllocation updBufIdsAllocation{ m_pDynamicUploadHeap->Allocate(updBufIdsSize) };
		memcpy(updBufIdsAllocation.cpuAddress, m_updBufIds.data(), updBufIdsSize);
		m_updBufIds.clear();

		size_t updBufSize{ updCnt * sizeof(T) };
		DynamicAllocation updBufAllocation{ m_pDynamicUploadHeap->Allocate(updBufSize) };
		memcpy(updBufAllocation.cpuAddress, m_updBuf.data(), updBufSize);
		m_updBuf.clear();

		std::shared_ptr<CommandList> pCommandListDirect{
			pCommandQueueDirect->GetCommandList(pDevice)
		};
		static const size_t threadBlockSize{ 128 };
		m_buffer.GetResource()->ResourceTransition(pCommandListDirect, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_pUpdater->Dispatch(
			pCommandListDirect,
			(updCnt + threadBlockSize - 1) / threadBlockSize, 1, 1,
			[&](std::shared_ptr<CommandList> pCommandList, UINT& rootParamId) {
				auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };
				pD3D12CommandList->SetComputeRoot32BitConstant(rootParamId++, updCnt, 0);
				pD3D12CommandList->SetComputeRootShaderResourceView(rootParamId++, updBufIdsAllocation.gpuAddress);
				pD3D12CommandList->SetComputeRootShaderResourceView(rootParamId++, updBufAllocation.gpuAddress);
				pD3D12CommandList->SetComputeRootUnorderedAccessView(
					rootParamId++,
					m_buffer.m_pResource->GetResource()->GetGPUVirtualAddress()
				);
			}
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	}
};

template <typename T>
class InstUploadBufferUpdater : public BufferUpdater<T> {
public:
	InstUploadBufferUpdater(
		Buffer<T>& buffer
	) : BufferUpdater<T>(buffer) {
		assert(m_buffer.m_heapData.heapType == D3D12_HEAP_TYPE_UPLOAD);
	}

	virtual void SetUpdateAll(T* pData, size_t count) override {
		void* pDst{};
		ThrowIfFailed(m_buffer.m_pResource->GetResource()->Map(0, nullptr, &pDst));
		memcpy(static_cast<std::byte*>(pDst), pData, count * sizeof(T));
	}
	virtual void SetUpdateAt(size_t id, const T& data) override {
		void* pDst{};
		ThrowIfFailed(m_buffer.m_pResource->GetResource()->Map(0, &CD3DX12_RANGE(id, id + 1), &pDst));
		memcpy(static_cast<std::byte*>(pDst), &data, sizeof(T));
	}
	virtual void PerformUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) override {}
};
