#include "DDSTexture.h"

DDSTexture::DDSTexture(
	const std::wstring& filename,
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<CommandQueue> pCommandQueueCopy,
	std::shared_ptr<CommandQueue> pCommandQueueDirect
) {
	LoadFromDDS(filename, pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect);
}

void DDSTexture::LoadFromDDS(
	const std::wstring& filename,
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<CommandQueue> pCommandQueueCopy,
	std::shared_ptr<CommandQueue> pCommandQueueDirect
) {
	// load texture from dds
	DirectX::ScratchImage image{};
	ThrowIfFailed(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
	
	// create texture resource
	CreateResource(
		pAllocator,
		HeapData{ D3D12_HEAP_TYPE_DEFAULT },
		ResourceData{
			CD3DX12_RESOURCE_DESC::Tex2D(
				image.GetMetadata().format,
				image.GetMetadata().width,
				image.GetMetadata().height,
				static_cast<UINT16>(image.GetMetadata().arraySize),
				static_cast<UINT16>(image.GetMetadata().mipLevels)
			),
			D3D12_RESOURCE_STATE_COPY_DEST
		}
	);
	
	// upload texture
	std::vector<D3D12_SUBRESOURCE_DATA> subresources{};
	subresources.reserve(image.GetMetadata().mipLevels);
	for (size_t i{}; i < image.GetMetadata().mipLevels; ++i) {
		if (const DirectX::Image* pMip{ image.GetImage(i, 0, 0) }; pMip) {
			subresources.emplace_back(pMip->pixels, pMip->rowPitch, pMip->slicePitch);
		}
	}

	std::shared_ptr<CommandList> pCommandListCopy{
		pCommandQueueCopy->GetCommandList(pDevice)
	};
	std::shared_ptr<GPUResource> pIntermediate{};
	UpdateSubresource(
		pAllocator,
		pCommandListCopy,
		0,
		subresources.size(),
		subresources.data(),
		pIntermediate
	);
	pCommandQueueCopy->ExecuteCommandListImmediately(pCommandListCopy);

	std::shared_ptr<CommandList> pCommandListDirect{
		pCommandQueueDirect->GetCommandList(pDevice)
	};
	ResourceTransition(
		pCommandListDirect->m_pCommandList,
		GetResource(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
	pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
}
