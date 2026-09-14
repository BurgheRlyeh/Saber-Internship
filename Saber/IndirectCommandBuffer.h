#pragma once

#include "Headers.h"

#include "Buffer.h"
#include "CommandList.h"
#include "Device.h"
#include "DeviceContext.h"
#include "GPUResource.h"

template <IndirectCommandConcept IndirectCommand>
class IndirectCommandBuffer : public Buffer<IndirectCommand> {
	Microsoft::WRL::ComPtr<D3D12CommandSignature> m_pCommandSignature{};

	// What the culling pass leaves behind: the commands that survived, packed from
	// the start of the buffer, and how many of them there are
	std::shared_ptr<GPUResource> m_pCulledCommands{};
	std::shared_ptr<GPUResource> m_pCulledCommandsCount{};

public:
	IndirectCommandBuffer(
		const std::wstring& renderSubsystemName,
		std::shared_ptr<DeviceContext> pDeviceContext,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		Microsoft::WRL::ComPtr<D3D12RootSignature> pRootSignature,
		uint32_t capacity
	) : Buffer<IndirectCommand>(
		renderSubsystemName,
		pDeviceContext,
		capacity,
		GPUResource::AllocationDesc{ .heapType{ D3D12_HEAP_TYPE_DEFAULT } },
		GPUResource::ResourceDesc{
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

		CreateCulledBuffers(pDeviceContext->GetDevice(), renderSubsystemName);
	}

	void Execute(
		std::shared_ptr<CommandList> pCommandList,
		size_t commandsCount
	) {
		assert(commandsCount <= GetCapacity());
		if (!commandsCount) {
			return;
		}

		m_pResource->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		pCommandList->GetD3D12CommandList()->ExecuteIndirect(
			m_pCommandSignature.Get(),
			static_cast<UINT>(commandsCount),
			m_pResource->GetD3D12Resource().Get(),
			0,
			nullptr,
			0
		);
	}

	// What the culling pass binds
	D3D12_GPU_VIRTUAL_ADDRESS GetSourceCommandsAddress() const {
		return m_pResource->GetD3D12Resource()->GetGPUVirtualAddress();
	}
	D3D12_GPU_VIRTUAL_ADDRESS GetCulledCommandsAddress() const {
		return m_pCulledCommands->GetD3D12Resource()->GetGPUVirtualAddress();
	}
	D3D12_GPU_VIRTUAL_ADDRESS GetCulledCommandsCountAddress() const {
		return m_pCulledCommandsCount->GetD3D12Resource()->GetGPUVirtualAddress();
	}
	std::shared_ptr<GPUResource> GetCulledCommandsCount() const {
		return m_pCulledCommandsCount;
	}

	// Source commands become readable and the count goes back to zero, so the pass
	// can append from the start
	void PrepareForCulling(std::shared_ptr<CommandList> pCommandList) {
		m_pResource->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_pCulledCommands->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		m_pCulledCommandsCount->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
		m_pCulledCommandsCount->ResetCounter(pCommandList, 0);
		m_pCulledCommandsCount->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	// How many commands survived is known only on the GPU, so the upper bound is the
	// whole buffer and the count buffer decides the rest
	void ExecuteCulled(std::shared_ptr<CommandList> pCommandList) {
		m_pCulledCommands->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		m_pCulledCommandsCount->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

		pCommandList->GetD3D12CommandList()->ExecuteIndirect(
			m_pCommandSignature.Get(),
			static_cast<UINT>(GetCapacity()),
			m_pCulledCommands->GetD3D12Resource().Get(),
			0,
			m_pCulledCommandsCount->GetD3D12Resource().Get(),
			0
		);
	}

protected:
	void CreateCulledBuffers(
		std::shared_ptr<Device> pDevice,
		const std::wstring& name
	) {
		m_pCulledCommands = std::make_shared<GPUResource>(
			name + L"/CulledCommands",
			pDevice,
			GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
			GPUResource::ResourceDesc{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					GetCapacity() * sizeof(IndirectCommand),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
			}
		);

		m_pCulledCommandsCount = std::make_shared<GPUResource>(
			name + L"/CulledCommandsCount",
			pDevice,
			GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
			GPUResource::ResourceDesc{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					sizeof(UINT),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) },
				.resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
			}
		);
	}

	static Microsoft::WRL::ComPtr<D3D12CommandSignature> CreateCommandSignature(
		std::shared_ptr<Device> pDevice,
		const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc,
		const Microsoft::WRL::ComPtr<D3D12RootSignature>& pRootSignature
	) {
		assert(commandSignatureDesc.ByteStride == sizeof(IndirectCommand));

		Microsoft::WRL::ComPtr<D3D12CommandSignature> pCommandSignature{};
		ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandSignature(
			&commandSignatureDesc,
			pRootSignature.Get(),
			IID_PPV_ARGS(&pCommandSignature)
		));

		return pCommandSignature;
	}
};
