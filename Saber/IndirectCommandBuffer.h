#pragma once

#include "Headers.h"

#include <bit>
#include <random>

#include "ConstantBuffer.h"
#include "ComputeObject.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"
#include "MeshRenderObject.h"

template <typename IndirectCommand>
class IndirectCommandBuffer {
protected:
	std::shared_ptr<DescriptorHeapManager> m_pDescHeapManagerCbvSrvUav{};
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};
	std::shared_ptr<GPUResource> m_pIndirectCommandBuffer{};
	std::shared_ptr<DescHeapRange> m_pDescHeapRangeUav{};
	
	uint32_t m_size{};
	uint32_t m_capacity{};

public:
	IndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName,
		uint32_t capacity
	) : m_pDescHeapManagerCbvSrvUav(pDescHeapManagerCbvSrvUav),
		m_capacity(std::bit_ceil(capacity))
	{
		m_pCommandSignature = CreateCommandSignature(
			pDevice,
			commandSignatureDesc,
			pRootSignature
		);

		m_pDescHeapRangeUav = pDescHeapManagerCbvSrvUav->AllocateRange(
			(renderObjectName + L"/IndirectCommandBuffer/Uav").c_str(),
			1,
			D3D12_DESCRIPTOR_RANGE_TYPE_UAV
		);

		m_capacity = AlignSize(
			m_capacity * sizeof(IndirectCommand),
			D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT
		) / sizeof(IndirectCommand);
		m_pIndirectCommandBuffer = CreateBuffer(pAllocator, m_capacity);
		m_pIndirectCommandBuffer->CreateUnorderedAccessView(
			pDevice,
			m_pDescHeapRangeUav->GetNextCpuHandle(),
			&GetUavDesc(m_capacity)
		);
	}

	virtual void SetUpdateAll(IndirectCommand* indirectCommands, size_t count) = 0;
	virtual void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) = 0;
	virtual void PerformUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) = 0;

	void Execute(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList) {
		pCommandList->ExecuteIndirect(
			m_pCommandSignature.Get(),
			m_capacity,
			m_pIndirectCommandBuffer->GetResource().Get(),
			0,
			nullptr,
			0
		);
	}

protected:
	static Microsoft::WRL::ComPtr<ID3D12CommandSignature> CreateCommandSignature(
		const Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		const Microsoft::WRL::ComPtr<ID3D12RootSignature>& pRootSignature
	) {
		assert(commandSignatureDesc.ByteStride == sizeof(IndirectCommand));

		Microsoft::WRL::ComPtr<ID3D12CommandSignature> pCommandSignature{};
		ThrowIfFailed(pDevice->CreateCommandSignature(
			&commandSignatureDesc,
			pRootSignature.Get(),
			IID_PPV_ARGS(&pCommandSignature)
		));

		return pCommandSignature;
	}

	static std::shared_ptr<GPUResource> CreateBuffer(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		uint32_t capacity,
		const D3D12_RESOURCE_STATES& initState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	) {
		uint32_t alignedSize{ AlignSize(
			capacity * sizeof(IndirectCommand),
			D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT
		) };
		//capacity = alignedSize / sizeof(IndirectCommand);
		return std::make_shared<GPUResource>(
			pAllocator,
			GPUResource::HeapData{.heapType{ D3D12_HEAP_TYPE_DEFAULT } },
			GPUResource::ResourceData{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					alignedSize,
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ initState }
			}
		);
	}

	static D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc(const size_t capacity) {
		return D3D12_UNORDERED_ACCESS_VIEW_DESC{
			.ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
			.Buffer{
				.NumElements{ static_cast<UINT>(capacity) },
				.StructureByteStride{ sizeof(IndirectCommand) }
			}
		};
	}

	bool Expand(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		uint32_t numElements
	) {
		if (numElements <= m_capacity) {
			return false;
		}

		std::shared_ptr<CommandList> pCommandListDirect{
			pCommandQueueDirect->GetCommandList(pDevice)
		};
		ResourceTransition(
			pCommandListDirect->m_pCommandList,
			m_pIndirectCommandBuffer->GetResource(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);

		uint32_t newCapacity{ std::bit_ceil(numElements) };
		std::shared_ptr<GPUResource> pIndirectCommandBufferNew{
			CreateBuffer(pAllocator, newCapacity, D3D12_RESOURCE_STATE_COPY_DEST)
		};

		std::shared_ptr<CommandList> pCommandListCopy{
			pCommandQueueCopy->GetCommandList(pDevice)
		};
		pCommandListCopy->m_pCommandList->CopyBufferRegion(
			pIndirectCommandBufferNew->GetResource().Get(),
			0,
			m_pIndirectCommandBuffer->GetResource().Get(),
			0,
			m_capacity * sizeof(IndirectCommand)
		);
		pCommandQueueCopy->ExecuteCommandListImmediately(pCommandListCopy);

		pCommandListDirect = pCommandQueueDirect->GetCommandList(pDevice);
		ResourceTransition(
			pCommandListDirect->m_pCommandList,
			m_pIndirectCommandBuffer->GetResource(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);

		pIndirectCommandBufferNew->CreateUnorderedAccessView(
			pDevice,
			m_pDescHeapRangeUav->GetCpuHandle(),
			&GetUavDesc(newCapacity)
		);
		m_capacity = newCapacity;
		m_pIndirectCommandBuffer = pIndirectCommandBufferNew;

		return true;
	}
};

template <typename IndirectCommand>
class StaticIndirectCommandBuffer : public IndirectCommandBuffer<IndirectCommand> {
	std::vector<IndirectCommand> m_indirectCommands{};

	std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeap{};

public:
	StaticIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName,
		size_t capacity,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap
	) : IndirectCommandBuffer<IndirectCommand>(
		pDevice,
		pAllocator,
		commandSignatureDesc,
		pRootSignature,
		pDescHeapManagerCbvSrvUav,
		renderObjectName + L"Static",
		capacity
	), m_pDynamicUploadHeap(pDynamicUploadHeap) {
		m_indirectCommands.resize(m_capacity);
	}

	void SetUpdateAll(IndirectCommand* indirectCommands, size_t count) override {
		if (count > m_capacity) {
			m_indirectCommands.resize(std::bit_ceil(count));
		}
		for (size_t i{}; i < count; ++i) {
			m_indirectCommands[i] = indirectCommands[i];
		}
	}

	void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) override {
		if (id >= m_indirectCommands.size()) {
			m_indirectCommands.resize(std::bit_ceil(id + 1));
		}
		m_indirectCommands[id] = indirectCommand;
	}

	void PerformUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) override {
		if (m_indirectCommands.size() > m_capacity) {
			IndirectCommandBuffer<IndirectCommand>::Expand(
				pDevice,
				pAllocator,
				pCommandQueueCopy,
				pCommandQueueDirect,
				m_indirectCommands.size()
			);
		}

		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ m_indirectCommands.data() },
			.RowPitch{ static_cast<UINT>(m_indirectCommands.size()) * sizeof(IndirectCommand) },
			.SlicePitch{ subresData.RowPitch }
		};

		std::shared_ptr<CommandList> pCommandListDirect{
			pCommandQueueDirect->GetCommandList(pDevice)
		};
		ResourceTransition(
			pCommandListDirect->m_pCommandList,
			m_pIndirectCommandBuffer->GetResource(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COPY_DEST
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);

		DynamicAllocation intermediateAllocation{
			m_pDynamicUploadHeap->Allocate(m_capacity * sizeof(IndirectCommand))
		};

		std::shared_ptr<CommandList> pCommandListCopy{
			pCommandQueueCopy->GetCommandList(pDevice)
		};
		UpdateSubresources(
			pCommandListCopy->m_pCommandList.Get(),
			m_pIndirectCommandBuffer->GetResource().Get(),
			intermediateAllocation.pBuffer->GetResource().Get(),
			intermediateAllocation.offset,
			0,
			1,
			&subresData
		);
		pCommandQueueCopy->ExecuteCommandListImmediately(pCommandListCopy);

		pCommandListDirect = pCommandQueueDirect->GetCommandList(pDevice);
		ResourceTransition(
			pCommandListDirect->m_pCommandList,
			m_pIndirectCommandBuffer->GetResource(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	}
};

template <typename IndirectCommand>
class DynamicIndirectCommandBuffer : public IndirectCommandBuffer<IndirectCommand> {
	std::vector<UINT> m_updBufIds{};
	std::vector<IndirectCommand> m_updBuf{};
	size_t m_updMaxId{};

	std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeap{};
	std::shared_ptr<ComputeObject> m_pIndirectUpdater{};

public:
	DynamicIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName,
		size_t capacity,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
		std::shared_ptr<ComputeObject> pIndirectUpdater
	) : IndirectCommandBuffer<IndirectCommand>(
			pDevice,
			pAllocator,
			commandSignatureDesc,
			pRootSignature,
			pDescHeapManagerCbvSrvUav,
			renderObjectName + L"Dynamic",
			capacity
		),
		m_pDynamicUploadHeap(pDynamicUploadHeap),
		m_pIndirectUpdater(pIndirectUpdater)
	{
		m_updBufIds.reserve(m_capacity);
		m_updBuf.reserve(m_capacity);
		m_updMaxId = m_capacity - 1;
	}

	void SetDynamicUploadHeap(const std::shared_ptr<DynamicUploadHeap>& pDynamicUploadHeap) {
		m_pDynamicUploadHeap = pDynamicUploadHeap;
	}

	void SetIndirectUpdater(const std::shared_ptr<ComputeObject>& pIndirectUpdater) {
		m_pIndirectUpdater = pIndirectUpdater;
	}

	void SetUpdateAll(IndirectCommand* indirectCommands, size_t count) override {
		if (count > m_capacity) {
			m_updBufIds.reserve(std::bit_ceil(count));
			m_updBuf.reserve(std::bit_ceil(count));
			m_updMaxId = count;
		}
		for (size_t i{}; i < count; ++i) {
			m_updBufIds.push_back(i);
			m_updBuf.push_back(indirectCommands[i]);
		}
	}

	void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) override {
		m_updBufIds.push_back(id);
		m_updBuf.push_back(indirectCommand);
		m_updMaxId = std::max<size_t>(m_updMaxId, id);
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

		if (m_updMaxId >= m_capacity) {
			IndirectCommandBuffer<IndirectCommand>::Expand(
				pDevice,
				pAllocator,
				pCommandQueueCopy,
				pCommandQueueDirect,
				m_updMaxId + 1
			);
		}
		m_updMaxId = m_capacity - 1;

		size_t updBufIdsSize{ updCnt * sizeof(UINT) };
		DynamicAllocation updBufIdsAllocation{ m_pDynamicUploadHeap->Allocate(updBufIdsSize) };
		memcpy(updBufIdsAllocation.cpuAddress, m_updBufIds.data(), updBufIdsSize);
		m_updBufIds.clear();

		size_t updBufSize{ updCnt * sizeof(IndirectCommand) };
		DynamicAllocation updBufAllocation{ m_pDynamicUploadHeap->Allocate(updBufSize) };
		memcpy(updBufAllocation.cpuAddress, m_updBuf.data(), updBufSize);
		m_updBuf.clear();

		std::shared_ptr<CommandList> pCommandListDirect{
			pCommandQueueDirect->GetCommandList(pDevice)
		};
		static const size_t threadBlockSize{ 128 };
		m_pIndirectUpdater->Dispatch(
			pCommandListDirect->m_pCommandList,
			static_cast<UINT>(std::ceil(updCnt / float(threadBlockSize))), 1, 1,
			[&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList, UINT& rootParamId) {
				pCommandList->SetComputeRoot32BitConstant(rootParamId++, updCnt, 0);
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, updBufIdsAllocation.gpuAddress);
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, updBufAllocation.gpuAddress);
				
				pCommandList->SetDescriptorHeaps(1, m_pDescHeapManagerCbvSrvUav->GetDescriptorHeap().GetAddressOf());
				pCommandList->SetComputeRootDescriptorTable(rootParamId++,
					m_pDescHeapRangeUav->GetGpuHandle()
				);
			}
		);

		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	}
};
