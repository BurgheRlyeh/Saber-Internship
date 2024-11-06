#pragma once

#include "Headers.h"

#include <random>

#include "ConstantBuffer.h"
#include "ComputeObject.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"
#include "MeshRenderObject.h"
#include "ModelBuffers.h"

template <typename IndirectCommand>
class IndirectCommandBuffer {
protected:
	std::shared_ptr<DescriptorHeapManager> m_pDescHeapManagerCbvSrvUav{};
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};
	std::shared_ptr<GPUResource> m_pIndirectCommandBuffer{};
	std::shared_ptr<DescHeapRange> m_pDescHeapRangeUav{};

	size_t m_size{};
	size_t m_capacity{};

public:
	IndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName,
		size_t capacity
	) : m_pDescHeapManagerCbvSrvUav(pDescHeapManagerCbvSrvUav), m_capacity(capacity) {
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
		
		m_pIndirectCommandBuffer = CreateBuffer(pAllocator, m_capacity);
		m_pIndirectCommandBuffer->CreateUnorderedAccessView(
			pDevice,
			m_pDescHeapRangeUav->GetNextCpuHandle(),
			&GetUavDesc(m_capacity)
		);
	}

	virtual void SetUpdateAll(IndirectCommand* indirectCommands) = 0;
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
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature
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
		const size_t capacity,
		const D3D12_RESOURCE_STATES& initState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	) {
		return std::make_shared<GPUResource>(
			pAllocator,
			GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
			GPUResource::ResourceData{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					capacity * sizeof(IndirectCommand),
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
		size_t numElements
	) {
		if (numElements <= m_capacity) {
			return false;
		}

		size_t newCapacity{ m_capacity << 1 };
		while (newCapacity < numElements) {
			newCapacity <<= 1;
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

public:
	StaticIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName,
		size_t capacity
	) : IndirectCommandBuffer<IndirectCommand>(
		pDevice,
		pAllocator,
		commandSignatureDesc,
		pRootSignature,
		pDescHeapManagerCbvSrvUav,
		renderObjectName + L"Static",
		capacity
	) {
		m_indirectCommands.reserve(capacity);
	}

	void SetUpdateAll(IndirectCommand* indirectCommands) override {
		IndirectCommand* pIndirectCommand{ indirectCommands };
		for (size_t i{}; i < m_capacity; ++i) {
			m_indirectCommands[i] = pIndirectCommand ? *pIndirectCommand++ : IndirectCommand{};
		}
	}

	void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) override {
		m_indirectCommands[id] = indirectCommand;
	}

	void PerformUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) override {
		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ m_indirectCommands.data() },
			.RowPitch{ m_capacity * sizeof(IndirectCommand) },
			.SlicePitch{ m_capacity * sizeof(IndirectCommand) }
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

		std::shared_ptr<CommandList> pCommandListCopy{
			pCommandQueueCopy->GetCommandList(pDevice)
		};
		std::shared_ptr<GPUResource> pIntermediate{ m_pIndirectCommandBuffer->UpdateSubresource(
			pAllocator,
			pCommandListCopy,
			0,
			1,
			&subresData
		) };
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
	}

	void SetDynamicUploadHeap(const std::shared_ptr<DynamicUploadHeap>& pDynamicUploadHeap) {
		m_pDynamicUploadHeap = pDynamicUploadHeap;
	}

	void SetIndirectUpdater(const std::shared_ptr<ComputeObject>& pIndirectUpdater) {
		m_pIndirectUpdater = pIndirectUpdater;
	}

	virtual void SetUpdateAll(IndirectCommand* indirectCommands) override {
		IndirectCommand* pIndirectCommand{ indirectCommands };
		for (size_t i{}; i < m_capacity; ++i) {
			if (pIndirectCommand) {
				m_updBufIds.push_back(i);
				m_updBuf.push_back(*pIndirectCommand++);
			}
		}
	}

	virtual void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) {
		m_updBufIds.push_back(id);
		m_updBuf.push_back(indirectCommand);
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

		if (updCnt > m_capacity) {
			IndirectCommandBuffer<IndirectCommand>::Expand(
				pDevice,
				pAllocator,
				pCommandQueueCopy,
				pCommandQueueDirect,
				updCnt
			);
		}

		size_t updBufIdsSize{ updCnt * sizeof(UINT) };
		DynamicAllocation m_updBufIdsAllocation{ m_pDynamicUploadHeap->Allocate(updBufIdsSize) };
		memcpy(m_updBufIdsAllocation.cpuAddress, m_updBufIds.data(), updBufIdsSize);
		m_updBufIds.clear();

		size_t updBufSize{ updCnt * sizeof(IndirectCommand) };
		DynamicAllocation m_updBufAllocation{ m_pDynamicUploadHeap->Allocate(updBufSize) };
		memcpy(m_updBufAllocation.cpuAddress, m_updBuf.data(), updBufSize);
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
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, m_updBufIdsAllocation.gpuAddress);
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, m_updBufAllocation.gpuAddress);
				
				pCommandList->SetDescriptorHeaps(1, m_pDescHeapManagerCbvSrvUav->GetDescriptorHeap().GetAddressOf());
				pCommandList->SetComputeRootDescriptorTable(rootParamId++,
					m_pDescHeapRangeUav->GetGpuHandle()
				);
			}
		);

		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	}
};
