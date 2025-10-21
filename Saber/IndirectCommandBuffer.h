#pragma once

#include "Headers.h"

#include <bit>
#include <random>

#include "Buffer.h"
#include "ConstantBuffer.h"
#include "ComputeObject.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DynamicUploadRingBuffer.h"
#include "GPUResource.h"
#include "MeshRenderObject.h"

template <IndirectCommandConcept IndirectCommand>
class IndirectCommandBuffer : public Buffer<IndirectCommand> {
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};

public:
	IndirectCommandBuffer(
		const std::wstring& renderSubsystemName,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		uint32_t capacity
	) : Buffer<IndirectCommand>(
			renderSubsystemName,
			pDevice,
			pAllocator,
			nullptr,
			pDescHeapManagerCbvSrvUav,
			capacity,
			GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
			GPUResource::ResourceData{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					capacity * sizeof(IndirectCommand),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
			}
		) {
		m_pCommandSignature = CreateCommandSignature(
			pDevice,
			commandSignatureDesc,
			pRootSignature
		);
	}

	void Execute(std::shared_ptr<CommandList> pCommandList) {
		pCommandList->GetD3D12CommandList()->ExecuteIndirect(
			m_pCommandSignature.Get(),
			m_capacity,
			m_pResource->GetResource().Get(),
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
};
