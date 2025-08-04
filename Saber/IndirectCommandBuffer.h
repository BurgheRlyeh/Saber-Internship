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
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};

	std::shared_ptr<GPUResource> m_pIndirectCommandBuffer{};
	std::shared_ptr<DescHeapRange> m_pDescHeapRangeSrv{};

	std::shared_ptr<GPUResource> m_pAppendIndirectCommandBuffer{};
	std::shared_ptr<DescHeapRange> m_pDescHeapRangeUav{};
	
	uint32_t m_size{};
	uint32_t m_capacity{};
	uint32_t m_counterOffset{};

public:
	IndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName,
		uint32_t capacity
	) {
		m_pCommandSignature = CreateCommandSignature(
			pDevice,
			commandSignatureDesc,
			pRootSignature
		);

		m_pDescHeapRangeSrv = pDescHeapManagerCbvSrvUav->AllocateRange(
			(renderObjectName + L"/IndirectCommandBuffer/Srv").c_str(),
			1,
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV
		);
		m_pDescHeapRangeUav = pDescHeapManagerCbvSrvUav->AllocateRange(
			(renderObjectName + L"/IndirectCommandBuffer/Uav").c_str(),
			1,
			D3D12_DESCRIPTOR_RANGE_TYPE_UAV
		);

		CreateBuffersAndViews(pDevice, pAllocator, capacity);
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

	void CreateBuffersAndViews(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		uint32_t numElements
	) {
		m_pDescHeapRangeSrv->Clear();
		m_pDescHeapRangeUav->Clear();

		uint32_t bufferSize{ AlignSize(
			std::bit_ceil(numElements) * sizeof(IndirectCommand),
			D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT
		) };
		m_capacity = bufferSize / sizeof(IndirectCommand);
		m_counterOffset = bufferSize;

		m_pIndirectCommandBuffer = std::make_shared<GPUResource>(
			pAllocator,
			GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
			GPUResource::ResourceData{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					bufferSize,
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
			}
		);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
			.ViewDimension{ D3D12_SRV_DIMENSION_BUFFER },
			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			.Buffer{
				.NumElements{ m_capacity },
				.StructureByteStride{ sizeof(IndirectCommand) }
	}
		};
		m_pIndirectCommandBuffer->CreateShaderResourceView(
			pDevice,
			m_pDescHeapRangeSrv->GetNextCpuHandle(),
			&srvDesc
		);

		m_pAppendIndirectCommandBuffer = std::make_shared<GPUResource>(
			pAllocator,
			GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
			GPUResource::ResourceData{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					bufferSize + sizeof(DirectX::XMUINT4),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
			}
		);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
			.ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
			.Buffer{
				.NumElements{ m_capacity },
				.StructureByteStride{ sizeof(IndirectCommand) },
				.CounterOffsetInBytes{ m_counterOffset }
			}
		};
		m_pAppendIndirectCommandBuffer->CreateUnorderedAccessView(
			pDevice,
			m_pDescHeapRangeUav->GetNextCpuHandle(),
			&uavDesc,
			m_pAppendIndirectCommandBuffer->GetResource()
		);
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

		uint32_t oldCapacity{ m_capacity };
		std::shared_ptr<GPUResource> pOldIndirectCommandBuffer{ m_pIndirectCommandBuffer };

		CreateBuffersAndViews(pDevice, pAllocator, numElements);

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

		std::shared_ptr<CommandList> pCommandListCopy{
			pCommandQueueCopy->GetCommandList(pDevice)
		};
		pCommandListCopy->m_pCommandList->CopyBufferRegion(
			m_pIndirectCommandBuffer->GetResource().Get(),
			0,
			pOldIndirectCommandBuffer->GetResource().Get(),
			0,
			oldCapacity * sizeof(IndirectCommand)
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
	size_t m_updSize{};

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
		m_updSize = m_capacity;
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
			m_updSize = count;
		}
		for (size_t i{}; i < count; ++i) {
			m_updBufIds.push_back(i);
			m_updBuf.push_back(indirectCommands[i]);
		}
	}

	void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) override {
		m_updBufIds.push_back(id);
		m_updBuf.push_back(indirectCommand);
		m_updSize = std::max<size_t>(m_updSize, id + 1);
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

		if (m_updSize > m_capacity) {
			IndirectCommandBuffer<IndirectCommand>::Expand(
				pDevice,
				pAllocator,
				pCommandQueueCopy,
				pCommandQueueDirect,
				m_updSize
			);
		}
		m_updSize = m_capacity;

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
			(updCnt + threadBlockSize - 1) / threadBlockSize, 1, 1,
			[&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList, UINT& rootParamId) {
				pCommandList->SetComputeRoot32BitConstant(rootParamId++, updCnt, 0);
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, updBufIdsAllocation.gpuAddress);
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, updBufAllocation.gpuAddress);
				pCommandList->SetComputeRootUnorderedAccessView(
					rootParamId++,
					m_pIndirectCommandBuffer->GetResource()->GetGPUVirtualAddress()
				);
			}
		);

		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	}
};
