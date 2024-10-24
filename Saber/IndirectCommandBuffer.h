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

template <typename IndirectCommand, size_t InstMaxCount>
class IndirectCommandBuffer {
protected:
	std::shared_ptr<DescriptorHeapManager> m_pDescHeapManagerCbvSrvUav{};
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};
	std::shared_ptr<GPUResource> m_pIndirectCommandBuffer{};
	std::shared_ptr<DescHeapRange> m_pDescHeapRangeUav{};

public:
	IndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName
	) : m_pDescHeapManagerCbvSrvUav(pDescHeapManagerCbvSrvUav) {
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

		InitializeBufferAndView(
			pDevice,
			pAllocator
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
			InstMaxCount,
			m_pIndirectCommandBuffer->GetResource().Get(),
			0,
			nullptr,
			0
		);
	}

protected:
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> CreateCommandSignature(
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

	void InitializeBufferAndView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
	) {
		m_pIndirectCommandBuffer = std::make_shared<GPUResource>(
			pAllocator,
			GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
			GPUResource::ResourceData{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					InstMaxCount * sizeof(IndirectCommand),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
			}
		);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
			.ViewDimension{ D3D12_UAV_DIMENSION_BUFFER },
			.Buffer{
				.NumElements{ static_cast<UINT>(InstMaxCount) },
				.StructureByteStride{ sizeof(IndirectCommand) }
			}
		};
		m_pIndirectCommandBuffer->CreateUnorderedAccessView(
			pDevice,
			m_pDescHeapRangeUav->GetNextCpuHandle(),
			&uavDesc
		);
	}
};

template <typename IndirectCommand, size_t InstMaxCount>
class StaticIndirectCommandBuffer : public IndirectCommandBuffer<IndirectCommand, InstMaxCount> {
	std::vector<IndirectCommand> m_indirectCommands{ InstMaxCount };

public:
	StaticIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName
	) : IndirectCommandBuffer<IndirectCommand, InstMaxCount>(
		pDevice,
		pAllocator,
		commandSignatureDesc,
		pRootSignature,
		pDescHeapManagerCbvSrvUav,
		renderObjectName + L"Static"
	) {}

	virtual void SetUpdateAll(IndirectCommand* indirectCommands) override {
		IndirectCommand* pIndirectCommand{ indirectCommands };
		for (size_t i{}; i < InstMaxCount; ++i) {
			m_indirectCommands[i] = pIndirectCommand ? *pIndirectCommand++ : IndirectCommand{};
		}
	}

	virtual void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) {
		m_indirectCommands[id] = indirectCommand;
	}

	virtual void PerformUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) {
		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ m_indirectCommands.data() },
			.RowPitch{ InstMaxCount * sizeof(IndirectCommand) },
			.SlicePitch{ InstMaxCount * sizeof(IndirectCommand) }
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

template <typename IndirectCommand, size_t InstMaxCount>
class DynamicIndirectCommandBuffer : public IndirectCommandBuffer<IndirectCommand, InstMaxCount> {
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
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
		std::shared_ptr<ComputeObject> pIndirectUpdater
	) : IndirectCommandBuffer<IndirectCommand, InstMaxCount>(
			pDevice,
			pAllocator,
			commandSignatureDesc,
			pRootSignature,
			pDescHeapManagerCbvSrvUav,
			renderObjectName + L"Dynamic"
		),
		m_pDynamicUploadHeap(pDynamicUploadHeap),
		m_pIndirectUpdater(pIndirectUpdater) 
	{
		m_updBufIds.reserve(InstMaxCount);
		m_updBuf.reserve(InstMaxCount);
	}

	virtual void SetUpdateAll(IndirectCommand* indirectCommands) override {
		IndirectCommand* pIndirectCommand{ indirectCommands };
		for (size_t i{}; i < InstMaxCount; ++i) {
			if (pIndirectCommand) {
				m_updBufIds.push_back(i);
				m_updBuf.push_back(*pIndirectCommand++);
			}
		}
	}

	virtual void SetUpdateAt(size_t id, const IndirectCommand& indirectCommand) {
		assert(id < InstMaxCount);
		m_updBufIds.push_back(id);
		m_updBuf.push_back(indirectCommand);
	}

	virtual void PerformUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) override {
		if (m_updBuf.empty()) {
			return;
		}

		size_t updBufIdsSize{ m_updBufIds.size() * sizeof(UINT) };
		DynamicAllocation m_updBufIdsAllocation{ m_pDynamicUploadHeap->Allocate(updBufIdsSize) };
		memcpy(m_updBufIdsAllocation.cpuAddress, m_updBufIds.data(), updBufIdsSize);

		size_t updBufSize{ m_updBuf.size() * sizeof(IndirectCommand) };
		DynamicAllocation m_updBufAllocation{ m_pDynamicUploadHeap->Allocate(updBufSize) };
		memcpy(m_updBufAllocation.cpuAddress, m_updBuf.data(), updBufSize);

		std::shared_ptr<CommandList> pCommandListDirect{
			pCommandQueueDirect->GetCommandList(pDevice)
		};

		static const size_t threadBlockSize{ 128 };
		m_pIndirectUpdater->Dispatch(
			pCommandListDirect->m_pCommandList,
			static_cast<UINT>(std::ceil(m_updBuf.size() / float(threadBlockSize))), 1, 1,
			[&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList, UINT& rootParamId) {
				pCommandList->SetComputeRoot32BitConstant(rootParamId++, m_updBuf.size(), 0);
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, m_updBufIdsAllocation.gpuAddress);
				pCommandList->SetComputeRootShaderResourceView(rootParamId++, m_updBufAllocation.gpuAddress);
				
				pCommandList->SetDescriptorHeaps(1, m_pDescHeapManagerCbvSrvUav->GetDescriptorHeap().GetAddressOf());
				pCommandList->SetComputeRootDescriptorTable(rootParamId++,
					m_pDescHeapRangeUav->GetGpuHandle()
				);
			}
		);

		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);

		m_updBuf.clear();
		m_updBufIds.clear();
	}
};
