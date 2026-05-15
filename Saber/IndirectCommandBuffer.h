/**
 * @file IndirectCommandBuffer.h
 * @brief GPU buffer for @c ExecuteIndirect command records with an associated
 *        @c ID3D12CommandSignature.
 *
 * @ref IndirectCommandBuffer<IndirectCommand> extends @ref Buffer<IndirectCommand>
 * with a D3D12 command signature so that the buffer can be consumed directly by
 * @c ExecuteIndirect.  The buffer is allocated on the default (GPU-local) heap
 * with the @c ALLOW_UNORDERED_ACCESS flag so it can be populated by a compute
 * shader.
 */
#pragma once

#include "Headers.h"

#include "Buffer.h"
#include "CommandList.h"
#include "Device.h"
#include "DeviceContext.h"
#include "GPUResource.h"

/**
 * @brief A @ref Buffer<IndirectCommand> paired with a @c ID3D12CommandSignature
 *        for GPU-driven rendering via @c ExecuteIndirect.
 *
 * @tparam IndirectCommand Indirect-command struct type; must satisfy
 *                         @ref IndirectCommandConcept.
 */
template <IndirectCommandConcept IndirectCommand>
class IndirectCommandBuffer : public Buffer<IndirectCommand> {
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{}; /**< @brief D3D12 command signature. */

public:
	/**
	 * @brief Constructs the buffer and creates the command signature.
	 * @param renderSubsystemName  Debug name prefix.
	 * @param pDeviceContext       Device context.
	 * @param commandSignatureDesc D3D12 command-signature descriptor; its
	 *                             @c ByteStride must equal @c sizeof(IndirectCommand).
	 * @param pRootSignature       Root signature associated with the command signature.
	 * @param capacity             Maximum number of indirect commands.
	 */
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
	}

	/**
	 * @brief Transitions the buffer to @c INDIRECT_ARGUMENT state (if needed) and
	 *        issues an @c ExecuteIndirect call for all commands in the buffer.
	 * @param pCommandList Direct command list on which to record the draw.
	 */
	void Execute(std::shared_ptr<CommandList> pCommandList) {
		if (m_pResource->GetState() != D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) {
			m_pResource->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		}
		pCommandList->GetD3D12CommandList()->ExecuteIndirect(
			m_pCommandSignature.Get(),
			GetCapacity(),
			m_pResource->GetD3D12Resource().Get(),
			0,
			nullptr,
			0
		);
	}

protected:
	/**
	 * @brief Creates and returns a D3D12 command signature.
	 * @param pDevice              Device wrapper.
	 * @param commandSignatureDesc Descriptor; asserted to have @c ByteStride == @c sizeof(IndirectCommand).
	 * @param pRootSignature       Root signature (may be @c nullptr for state-change-only signatures).
	 * @return Newly created @c ID3D12CommandSignature.
	 */
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
