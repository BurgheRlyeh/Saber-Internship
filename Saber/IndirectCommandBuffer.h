#pragma once

#include "Headers.h"

#include "Buffer.h"
#include "CommandList.h"
#include "Device.h"
#include "DeviceContext.h"
#include "GPUResource.h"

template <IndirectCommandConcept IndirectCommand>
class IndirectCommandBuffer : public Buffer<IndirectCommand> {
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};

public:
	IndirectCommandBuffer(
		const std::wstring& renderSubsystemName,
		std::shared_ptr<DeviceContext> pDeviceContext,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
		uint32_t capacity
	) : Buffer<IndirectCommand>(
		renderSubsystemName,
		pDeviceContext,
		capacity,
		GPUResource::HeapData{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
		GPUResource::ResourceData{
			.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
				capacity * sizeof(IndirectCommand),
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
			) }
		}
	) {
		m_pCommandSignature = CreateCommandSignature(
			pDeviceContext->GetDevice(),
			commandSignatureDesc,
			pRootSignature
		);
	}

	void Execute(std::shared_ptr<CommandList> pCommandList) {
		if (m_pResource->GetState() != D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) {
			m_pResource->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		}
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
		std::shared_ptr<Device> pDevice,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		const Microsoft::WRL::ComPtr<ID3D12RootSignature>& pRootSignature
	) {
		assert(commandSignatureDesc.ByteStride == sizeof(IndirectCommand));

		Microsoft::WRL::ComPtr<ID3D12CommandSignature> pCommandSignature{};
		ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandSignature(
			&commandSignatureDesc,
			pRootSignature.Get(),
			IID_PPV_ARGS(&pCommandSignature)
		));

		return pCommandSignature;
	}
};
