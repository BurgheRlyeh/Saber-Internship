#pragma once

#include "Headers.h"

#include "DirectXTex.h"

#include "GPUResource.h"

class Texture : public GPUResource {
	DirectX::ScratchImage m_image{};
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pTextureDescHeap{};

public:
	Texture(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
		, std::shared_ptr<CommandQueue> const& pCommandQueueCopy
		, std::shared_ptr<CommandQueue> const& pCommandQueueDirect
		, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
		, const wchar_t* filename
		, DirectX::DDS_FLAGS flags = DirectX::DDS_FLAGS_NONE
	) {
		// load texture from file
		ThrowIfFailed(DirectX::LoadFromDDSFile(filename, flags, nullptr, m_image));

		// create texture resource
		D3D12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
			m_image.GetMetadata().format,
			m_image.GetMetadata().width,
			m_image.GetMetadata().height
		) };

		CreateResource(
			pAllocator,
			resourceDesc,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_COPY_DEST
		);

		// create upload heap
		uint64_t textureMemorySize{};
		pDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureMemorySize);

		Microsoft::WRL::ComPtr<ID3D12Resource> textureBufferUploadHeap{};
		ThrowIfFailed(pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(textureMemorySize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureBufferUploadHeap)
		));

		// upload texture
		std::shared_ptr<CommandList> pCommandList{
			pCommandQueueCopy->GetCommandList(pDevice)
		};

		D3D12_SUBRESOURCE_DATA textureData{
			.pData{ m_image.GetImages()->pixels },
			.RowPitch{ static_cast<LONG_PTR>(m_image.GetImages()->rowPitch) },
			.SlicePitch{ static_cast<LONG_PTR>(m_image.GetImages()->slicePitch) }
		};
		UpdateSubresources(pCommandList->m_pCommandList.Get(), GetResource().Get(), textureBufferUploadHeap.Get(), 0, 0, 1, &textureData);

		pCommandQueueCopy->ExecuteCommandListImmediately(pCommandList);

		std::shared_ptr<CommandList> pCommandListDirect{
			pCommandQueueDirect->GetCommandList(pDevice)
		};

		pCommandListDirect->m_pCommandList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				GetResource().Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		);
		pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);

		// create SRV desc heap
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{
			.Type{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV },
			.NumDescriptors{ 1 },
			.Flags{ D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },
			.NodeMask{}
		};
		ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pTextureDescHeap)));

		// create SRV desc
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
			.Format{ m_image.GetImages()->format },
			.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			.Texture2D{
				.MipLevels{ 1 }
			}
		};
		pDevice->CreateShaderResourceView(
			GetResource().Get(),
			&srvDesc,
			m_pTextureDescHeap->GetCPUDescriptorHandleForHeapStart()
		);
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetTextureDescHeap() {
		return m_pTextureDescHeap;
	}
};