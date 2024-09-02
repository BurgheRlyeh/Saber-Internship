#include "Texture.h"

//Texture::Texture(Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_HEAP_TYPE& heapType, const D3D12_RESOURCE_STATES& resourceState, const D3D12_CLEAR_VALUE* pClearValue) {
//	CreateResource(
//		pAllocator,
//		resourceDesc,
//		D3D12_HEAP_TYPE_DEFAULT,
//		D3D12_RESOURCE_STATE_RENDER_TARGET,
//		pClearValue
//	);
//}

Texture::Texture(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
	std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const LPCWSTR& filename,
	D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle,
	DirectX::DDS_FLAGS flags
) {
	InitSRVFromDDSFile(
		pDevice,
		pCommandQueueCopy,
		pCommandQueueDirect,
		pAllocator,
		filename,
		pCPUDescHandle,
		flags
	);
}

//std::shared_ptr<Texture> Texture::CreateRenderTarget(
//	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
//	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
//	const D3D12_RESOURCE_DESC& resourceDesc,
//	D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHeap,
//	const D3D12_CLEAR_VALUE* pClearValue
//) {
//	std::shared_ptr<Texture> pTexture = std::make_shared<Texture>(
//		pAllocator,
//		resourceDesc,
//		D3D12_HEAP_TYPE_DEFAULT,
//		D3D12_RESOURCE_STATE_RENDER_TARGET,
//		pClearValue
//	);
//
//	pTexture->CreateRenderTargetView(pDevice, pCPUDescHeap);
//	return pTexture;
//}

void Texture::CreateRenderTargetView(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle, const D3D12_DESCRIPTOR_HEAP_FLAGS& flags) {
	if (!pCPUDescHandle) {
		CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, flags);
		pCPUDescHandle = &m_pTextureDescHeap->GetCPUDescriptorHandleForHeapStart();
	}
	pDevice->CreateRenderTargetView(
		GetResource().Get(),
		nullptr,
		*pCPUDescHandle
	);
}

void Texture::CreateShaderResourceView(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc, const D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle) {
	if (!pCPUDescHandle) {
		CreateDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		pCPUDescHandle = &m_pTextureDescHeap->GetCPUDescriptorHandleForHeapStart();
	}
	pDevice->CreateShaderResourceView(
		GetResource().Get(),
		&srvDesc,
		*pCPUDescHandle
	);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Texture::GetTextureDescHeap() const {
	return m_pTextureDescHeap;
}

void Texture::CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, const D3D12_DESCRIPTOR_HEAP_TYPE& type, const D3D12_DESCRIPTOR_HEAP_FLAGS& flags) {
	D3D12_DESCRIPTOR_HEAP_DESC desc{
		.Type{ type },
		.NumDescriptors{ 1 },
		.Flags{ flags }
	};

	pDevice->GetDeviceRemovedReason();
	ThrowIfFailed(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pTextureDescHeap)));
}

void Texture::InitSRVFromDDSFile(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<CommandQueue> const& pCommandQueueCopy, std::shared_ptr<CommandQueue> const& pCommandQueueDirect, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, const LPCWSTR& filename, D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle, DirectX::DDS_FLAGS flags) {
	DirectX::ScratchImage image{};

	// load texture from file
	ThrowIfFailed(DirectX::LoadFromDDSFile(filename, flags, nullptr, image));

	// create texture resource
	D3D12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
		image.GetMetadata().format,
		image.GetMetadata().width,
		static_cast<UINT>(image.GetMetadata().height)
	) };

	CreateResource(
		pAllocator,
		resourceDesc,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_COPY_DEST
	);

	// upload texture
	uint64_t uploadBufferSize{
		GetRequiredIntermediateSize(GetResource().Get(), 0, static_cast<UINT>(image.GetImageCount()))
	};
	GPUResource uploadBuffer{
		pAllocator,
		CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ
	};

	D3D12_SUBRESOURCE_DATA textureData{
		.pData{ image.GetImages()->pixels },
		.RowPitch{ static_cast<LONG_PTR>(image.GetImages()->rowPitch) },
		.SlicePitch{ static_cast<LONG_PTR>(image.GetImages()->slicePitch) }
	};

	std::shared_ptr<CommandList> pCommandListCopy{
		pCommandQueueCopy->GetCommandList(pDevice)
	};
	UpdateSubresources(
		pCommandListCopy->m_pCommandList.Get(),
		GetResource().Get(),
		uploadBuffer.GetResource().Get(),
		0,
		0,
		1,
		&textureData
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

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ image.GetImages()->format },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{
			.MipLevels{ 1 }
		}
	};

	CreateShaderResourceView(pDevice, srvDesc, pCPUDescHandle);
}
