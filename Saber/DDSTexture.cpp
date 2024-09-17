#include "DDSTexture.h"

DDSTexture::DDSTexture(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<CommandQueue> pCommandQueueCopy,
	std::shared_ptr<CommandQueue> pCommandQueueDirect,
	const LPCWSTR& filename
) {
	LoadFromDDS(pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect, filename);
	m_pSrvDesc = std::make_unique<D3D12_SHADER_RESOURCE_VIEW_DESC>(CreateSrvDesc());
}

const D3D12_SHADER_RESOURCE_VIEW_DESC* DDSTexture::GetSrvDesc() const {
	return m_pSrvDesc.get();
}

void DDSTexture::LoadFromDDS(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<CommandQueue> pCommandQueueCopy,
	std::shared_ptr<CommandQueue> pCommandQueueDirect,
	const LPCWSTR& filename
) {
	// load texture from dds
	DirectX::ScratchImage image{};
	ThrowIfFailed(DirectX::LoadFromDDSFile(filename, DirectX::DDS_FLAGS_NONE, &m_metadata, image));

	// create texture resource
	CreateResource(
		pAllocator,
		HeapData{ D3D12_HEAP_TYPE_DEFAULT },
		ResourceData{
			CD3DX12_RESOURCE_DESC::Tex2D(
				m_metadata.format,
				m_metadata.width,
				m_metadata.height
			),
			D3D12_RESOURCE_STATE_COPY_DEST
		}
	);

	// upload texture
	UINT imagesCount{ static_cast<UINT>(image.GetImageCount()) };
	GPUResource uploadBuffer{
		pAllocator,
		HeapData{ D3D12_HEAP_TYPE_UPLOAD },
		ResourceData{
			CD3DX12_RESOURCE_DESC::Buffer(
				GetRequiredIntermediateSize(GetResource().Get(), 0, imagesCount)
			),
			D3D12_RESOURCE_STATE_GENERIC_READ
		}
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
}

D3D12_SHADER_RESOURCE_VIEW_DESC DDSTexture::CreateSrvDesc() const {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ m_metadata.format },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING }
	};

	switch (m_metadata.dimension) {
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		srvDesc.Texture1D = { .MipLevels{ 1 } };	// static_cast<UINT>(m_metadata.mipLevels)
		break;

	case DirectX::TEX_DIMENSION_TEXTURE2D:
		if (m_metadata.IsCubemap()) {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube = { .MipLevels{ 1 } };
		}
		else {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D = { .MipLevels{ 1 } };
		}
		break;

	case DirectX::TEX_DIMENSION_TEXTURE3D:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D = { .MipLevels{ 1 } };
		break;
	}

	return srvDesc;
}
