#pragma once

#include "Headers.h"

#include <random>

#include "ConstantBuffer.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"
#include "MeshRenderObject.h"
#include "ModelBuffers.h"

template <typename IndirectCommand, size_t InstMaxCount>
class IndirectCommandBuffer {
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};
	std::shared_ptr<GPUResource> m_pIndirectCommandBuffer{};

	std::shared_ptr<DescHeapRange> m_pDescHeapRangeUav{};

	std::vector<IndirectCommand> m_indrectCommands{};

	//struct IndirectCommandUpdate {
	//	DirectX::XMUINT4 instanceId{};
	//	IndirectCommand indirectCommand{};
	//	IndirectCommandUpdate(size_t id, IndirectCommand command) {
	//		instanceId.x = id;
	//		indirectCommand = command;
	//	}
	//};
	//std::vector<IndirectCommandUpdate> m_updBuf{};
	//std::shared_ptr<DynamicUploadHeap> m_pUpdBufHeap{};
	//DynamicAllocation m_updBufAlloc{};
	//std::shared_ptr<DescHeapRange> m_pDescHeapRangeSrv{};
	//bool m_isUpd{};
	//size_t m_updCount{};
	//size_t m_updBufsCnt{};
	//size_t m_currUpdBufId{};

public:
	IndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		const std::wstring& renderObjectName
	) {
		m_pCommandSignature = CreateCommandSignature(
			pDevice,
			commandSignatureDesc,
			pRootSignature
		);

		m_pDescHeapRangeUav = pDescHeapManagerCbvSrvUav->AllocateRange(
			(renderObjectName + L"IndirectCommandBuffer").c_str(),
			1,
			D3D12_DESCRIPTOR_RANGE_TYPE_UAV
		);

		InitializeBufferAndView(
			pDevice,
			pAllocator
		);
	}

	void CopyToBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		IndirectCommand* indirectCommands
	) {
		D3D12_SUBRESOURCE_DATA subresData{
			.pData{ indirectCommands },
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

	void Execute(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList
	) {
		pCommandList->ExecuteIndirect(
			m_pCommandSignature.Get(),
			InstMaxCount,
			m_pIndirectCommandBuffer->GetResource().Get(),
			0,
			nullptr,
			0
		);
	}

	//void InitDynamicUpdateBuffer(
	//	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	//	std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
	//	const std::wstring& objectName,
	//	size_t framesCount
	//) {
	//	m_updBuf.reserve(InstMaxCount);
	//	m_pUpdBufHeap = std::make_shared<DynamicUploadHeap>(
	//		pAllocator,
	//		InstMaxCount * sizeof(IndirectCommand),
	//		true
	//	);
	//	m_updBufsCnt = framesCount;
	//	m_pDescHeapRangeSrv = pDescHeapManagerCbvSrvUav->AllocateRange(
	//		(objectName + L"IndirectCommandBuffer").c_str(),
	//		m_updBufsCnt,
	//		D3D12_DESCRIPTOR_RANGE_TYPE_SRV
	//	);
	//}
	//void UpdateAll(IndirectCommand* data) {
	//	IndirectCommand* p{ data };
	//	for (size_t i{}; i < InstMaxCount; ++i, ++p) {
	//		m_updBuf.emplace_back(i, *p);
	//	}
	//	m_isUpd = true;
	//}
	//void UpdateAt(size_t instId, IndirectCommand* data) {
	//	assert(m_updBuf.size() < InstMaxCount);
	//	m_updBuf.emplace_back(instId, *data);
	//	m_isUpd = true;
	//}
	//bool FinishUpdate(
	//	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
	//) {
	//	if (!m_isUpd) {
	//		return false;
	//	}
	//	m_isUpd = false;
	//	size_t updBufSize{ m_updBuf.size() * sizeof(IndirectCommandUpdate) };
	//	m_updBufAlloc = m_pUpdBufHeap->Allocate(updBufSize);
	//	memcpy(m_updBufAlloc.cpuAddress, m_updBuf.data(), updBufSize);
	//	m_currUpdBufId = (m_currUpdBufId + 1) % m_updBufsCnt;
	//	m_updBufAlloc.pBuffer->CreateShaderResourceView(
	//		pDevice,
	//		//m_pDescHeapRange->
	//	);
	//	return true;
	//}

private:
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
