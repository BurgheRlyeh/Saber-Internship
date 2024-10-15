#pragma once

#include "Headers.h"

#include "MeshRenderObject.h"

template <size_t InstancesMaxCount = 100>
class IndirectRenderObject : MeshRenderObject {
	struct IndirectCommand {
		D3D12_GPU_VIRTUAL_ADDRESS cbv{};
		D3D12_DRAW_ARGUMENTS drawArgs{};
	};
	std::vector<IndirectCommand> m_indirectCommands{ InstancesMaxCount };

	Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_pCommandSignature{};

public:
	IndirectRenderObject(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const DirectX::XMMATRIX& modelMatrix = DirectX::XMMatrixIdentity()
	) : MeshRenderObject(pDevice, pAllocator, modelMatrix) {
		D3D12_INDIRECT_ARGUMENT_DESC argsDescs[2]{
			{
				.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW },
				.ConstantBufferView{.RootParameterIndex{ 0 } }
			},
			{
				.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW }
			}
		};
	
		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{
			.ByteStride{ sizeof(IndirectCommand) },
			.NumArgumentDescs{ _countof(argsDescs) },
			.pArgumentDescs{ argsDescs }
		};

		//ThrowIfFailed(pDevice->CreateCommandSignature(
		//	&commandSignatureDesc,
		//	,
		//	IID_PPV_ARGS(&m_pCommandSignature)
		//));
	}
};