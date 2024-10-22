#include "DDSTexture.h"

DDSTexture::DDSTexture(
	const std::wstring& filename,
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<CommandQueue> pCommandQueueCopy,
	std::shared_ptr<CommandQueue> pCommandQueueDirect
) {
	LoadFromDDS(filename, pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect);
	m_pSrvDesc = std::make_unique<D3D12_SHADER_RESOURCE_VIEW_DESC>(CreateSrvDesc());
}

const D3D12_SHADER_RESOURCE_VIEW_DESC* DDSTexture::GetSrvDesc() const {
	return m_pSrvDesc.get();
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
	ThrowIfFailed(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_NONE, &m_metadata, image));

	// create texture resource
	CreateResource(
		pAllocator,
		HeapData{ D3D12_HEAP_TYPE_DEFAULT },
		ResourceData{
			CD3DX12_RESOURCE_DESC::Tex2D(
				m_metadata.format,
				m_metadata.width,
				m_metadata.height,
				static_cast<UINT16>(m_metadata.arraySize),
				static_cast<UINT16>(m_metadata.mipLevels)
			),
			D3D12_RESOURCE_STATE_COPY_DEST
		}
	);

	// upload texture
	std::vector<D3D12_SUBRESOURCE_DATA> subresources{};
	subresources.reserve(m_metadata.mipLevels);
	for (size_t i{}; i < m_metadata.mipLevels; ++i) {
		if (const DirectX::Image* pMip{ image.GetImage(i, 0, 0) }; pMip) {
			subresources.emplace_back(pMip->pixels, pMip->rowPitch, pMip->slicePitch);
		}
	}

	std::shared_ptr<CommandList> pCommandListCopy{
		pCommandQueueCopy->GetCommandList(pDevice)
	};
	std::shared_ptr<GPUResource> pIntermediate{ UpdateSubresource(
		pAllocator,
		pCommandListCopy,
		0,
		subresources.size(),
		subresources.data()
	) };
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

	UINT mipLevels{ static_cast<UINT>(m_metadata.mipLevels) };
	
	switch (m_metadata.dimension) {
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		srvDesc.Texture1D = { .MipLevels{ mipLevels } };
		break;

	case DirectX::TEX_DIMENSION_TEXTURE2D:
		if (m_metadata.IsCubemap()) {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube = { .MipLevels{ mipLevels } };
		}
		else {
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D = { .MipLevels{ mipLevels } };
		}
		break;

	case DirectX::TEX_DIMENSION_TEXTURE3D:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D = { .MipLevels{ mipLevels } };
		break;
	}

	return srvDesc;
}
