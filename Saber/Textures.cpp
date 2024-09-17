#include "Textures.h"

Textures::Textures(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice)
	: m_rtvHandleIncSize(pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV))
	, m_srvUavHandleIncSize(pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
{}

Textures::Textures(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	size_t texturesCount
) : Textures(pDevice) {
	CreateDescriptorHeaps(pDevice, texturesCount);
}

void Textures::LoadFromDDS(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	std::shared_ptr<CommandQueue> pCommandQueueCopy,
	std::shared_ptr<CommandQueue> pCommandQueueDirect,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const LPCWSTR* filenames,
	size_t texturesCount
) {
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescHandle{
		GetSrvUavDescHeap()->GetCPUDescriptorHandleForHeapStart()
	};
	for (size_t i{}; i < GetTexturesCount(); ++i) {
		auto pTex = std::make_shared<DDSTexture>(
			pDevice,
			pAllocator,
			pCommandQueueCopy,
			pCommandQueueDirect,
			filenames[i]
		);
		//pTex->CreateShaderResourceView(pDevice, cpuDescHandle);
		pTex->CreateShaderResourceView(pDevice, cpuDescHandle, pTex->GetSrvDesc());

		m_pTextures.push_back(pTex);

		cpuDescHandle.Offset(1, m_srvUavHandleIncSize);
	}
}

void Textures::Resize(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const D3D12_RESOURCE_DESC* resourceDesc,
	size_t texturesCount,
	const D3D12_CLEAR_VALUE* pClearValue
) {
	if (!m_pRTVDescHeap || !m_pSrvUavDescHeap) {
		return;
	}
	m_pTextures.clear();
	m_pTextures.reserve(texturesCount);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ GetCpuRtvDescHandle() };
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvUavHandle{ GetCpuSrvUavDescHandle() };

	//m_resourcesCounter = {};
	for (size_t i{}; i < texturesCount; ++i) {
		m_pTextures.push_back(std::make_shared<Texture>(
			pAllocator,
			GPUResource::HeapData{ D3D12_HEAP_TYPE_DEFAULT },
			GPUResource::ResourceData{ resourceDesc[i], D3D12_RESOURCE_STATE_RENDER_TARGET, pClearValue }
		));

		bool isRt{};
		if (resourceDesc[i].Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
			m_pTextures[i]->CreateRenderTargetView(
				pDevice,
				rtvHandle
			);
			isRt = true;
		}
		if (isRt) {
			rtvHandle.Offset(1, m_rtvHandleIncSize);
			//++m_resourcesCounter.rtvId;
		}
			
		bool isSrvUav{};
		if (!(resourceDesc[i].Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
			//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
			//	.Format{ resourceDesc[i].Format },
			//	.ViewDimension{ ResToSrvDim(resourceDesc[i].Dimension) },
			//	.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			//	.Texture2D{
			//		.MipLevels{ 1 }
			//	}
			//};
			m_pTextures[i]->CreateShaderResourceView(
				pDevice,
				srvUavHandle
			);
			isSrvUav = true;
		}
		if (resourceDesc[i].Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
				.Format{ resourceDesc[i].Format },
				.ViewDimension{ ResToUavDim(resourceDesc[i].Dimension) }
			};
			m_pTextures[i]->CreateUnorderedAccessView(
				pDevice,
				srvUavHandle,
				&uavDesc
			);
			isSrvUav = true;
		}
		if (isSrvUav) {
			srvUavHandle.Offset(1, m_srvUavHandleIncSize);
			//++m_resourcesCounter.srvUavId;
		}
	}
}

void Textures::CreateDescriptorHeaps(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	size_t texturesCount
) {
	m_pSrvUavDescHeap = CreateTextureDescHeap(
		pDevice,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		texturesCount,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
	m_pRTVDescHeap = CreateTextureDescHeap(
		pDevice,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		texturesCount
	);

	m_pTextures.clear();
	m_pTextures.reserve(texturesCount);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Textures::CreateTextureDescHeap(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_DESCRIPTOR_HEAP_TYPE& type,
	const size_t& texturesCount,
	const D3D12_DESCRIPTOR_HEAP_FLAGS& flags
) {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap{};
	D3D12_DESCRIPTOR_HEAP_DESC desc{ type, static_cast<UINT>(texturesCount), flags };
	ThrowIfFailed(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescHeap)));

	return pDescHeap;
}

UINT Textures::GetTexturesCount() const {
	return m_pSrvUavDescHeap->GetDesc().NumDescriptors;
}

std::shared_ptr<Texture> Textures::GetTexture(size_t textureId) const {
	return textureId < m_pTextures.size() ? m_pTextures[textureId] : nullptr;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Textures::GetSrvUavDescHeap() const {
	return m_pSrvUavDescHeap;
}
D3D12_CPU_DESCRIPTOR_HANDLE Textures::GetCpuSrvUavDescHandle(size_t textureId) const {
	return !m_pSrvUavDescHeap ? D3D12_CPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_pSrvUavDescHeap->GetCPUDescriptorHandleForHeapStart(),
		static_cast<INT>(textureId),
		m_srvUavHandleIncSize
	);
}
D3D12_GPU_DESCRIPTOR_HANDLE Textures::GetGpuSrvUavDescHandle(size_t textureId) const {
	return !m_pSrvUavDescHeap ? D3D12_GPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_pSrvUavDescHeap->GetGPUDescriptorHandleForHeapStart(),
		static_cast<INT>(textureId),
		m_srvUavHandleIncSize
	);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Textures::GetRtvDescHeap() const {
	return m_pRTVDescHeap;
}
D3D12_CPU_DESCRIPTOR_HANDLE Textures::GetCpuRtvDescHandle(size_t textureId) const {
	return !m_pRTVDescHeap ? D3D12_CPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_pRTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		static_cast<INT>(textureId),
		m_rtvHandleIncSize
	);
}
D3D12_GPU_DESCRIPTOR_HANDLE Textures::GetGpuRtvDescHandle(size_t textureId) const {
	return !m_pRTVDescHeap ? D3D12_GPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_pRTVDescHeap->GetGPUDescriptorHandleForHeapStart(),
		static_cast<INT>(textureId),
		m_rtvHandleIncSize
	);
}
