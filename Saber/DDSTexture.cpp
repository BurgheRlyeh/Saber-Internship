#include "DDSTexture.h"

#include "DirectXTex.h"

#include "CommandQueue.h"
#include "DeviceContext.h"

DDSTexture::DDSTexture(
	const std::wstring& filename,
	std::shared_ptr<DeviceContext> pDeviceContext,
	std::shared_ptr<CommandQueue> pCommandQueueDirect
) {
	LoadFromDDS(filename, pDeviceContext, pCommandQueueDirect);
}

void DDSTexture::LoadFromDDS(
	const std::wstring& filename,
	std::shared_ptr<DeviceContext> pDeviceContext,
	std::shared_ptr<CommandQueue> pCommandQueueDirect
) {
	// load texture from dds
	DirectX::ScratchImage image{};
	ThrowIfFailed(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
	
	// create texture resource
	CreateResource(
		filename,
		pDeviceContext->GetDevice(),
		HeapData{ D3D12_HEAP_TYPE_DEFAULT },
		ResourceData{
			CD3DX12_RESOURCE_DESC::Tex2D(
				image.GetMetadata().format,
				image.GetMetadata().width,
				image.GetMetadata().height,
				static_cast<UINT16>(image.GetMetadata().arraySize),
				static_cast<UINT16>(image.GetMetadata().mipLevels)
			)
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
		pCommandQueueDirect->GetCommandList(pDeviceContext->GetDevice())
	};
	ResourceTransition(pCommandListCopy, D3D12_RESOURCE_STATE_COPY_DEST);
	std::shared_ptr<GPUResource> pIntermediate{ CreateIntermediate(pDeviceContext->GetDevice(), 0, subresources.size())};
	UpdateSubresources(
		pCommandListCopy,
		pIntermediate,
		subresources.data(),
		0, 0, subresources.size()
	);
	pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListCopy);

	std::shared_ptr<CommandList> pCommandListDirect{
		pCommandQueueDirect->GetCommandList(pDeviceContext->GetDevice())
	};
	ResourceTransition(pCommandListDirect, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
}
