#include "Textures.h"

Textures::Textures(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice)
	: m_rtvHandleIncSize(pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV))
	, m_srvHandleIncSize(pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
{}

Textures::Textures(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const Type& type,
	size_t texturesCount
) : Textures(pDevice) {
	CreateDescriptorHeaps(pDevice, type, texturesCount);
}

void Textures::LoadFromDDS(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<CommandQueue> pCommandQueueCopy, std::shared_ptr<CommandQueue> pCommandQueueDirect, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, const LPCWSTR* filenames, size_t texturesCount) {
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescHandle{
		GetSRVDescHeap()->GetCPUDescriptorHandleForHeapStart()
	};
	for (size_t i{}; i < GetTexturesCount(); ++i) {
		m_pTextures[i] = std::make_shared<Texture>(
			pDevice,
			pCommandQueueCopy,
			pCommandQueueDirect,
			pAllocator,
			filenames[i],
			&cpuDescHandle
		);

		cpuDescHandle.Offset(1, m_srvHandleIncSize);
	}
}

void Textures::Resize(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const D3D12_RESOURCE_DESC& resourceDesc,
	const D3D12_CLEAR_VALUE* pClearValue
) {
	if (!m_pRTVDescHeap || !m_pSRVDescHeap) {
		return;
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ GetCpuRtvDescHandle() };
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle{ GetCpuSrvDescHandle() };

	for (auto& pTex : m_pTextures) {
		pTex = std::make_shared<Texture>(
			pAllocator,
			resourceDesc,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			pClearValue
		);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
			.Format{ resourceDesc.Format },
			.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			.Texture2D{
				.MipLevels{ 1 }
			}
		};
		pTex->CreateShaderResourceView(
			pDevice,
			srvDesc,
			&srvHandle
		);
		srvHandle.Offset(1, m_srvHandleIncSize);

		pTex->CreateRenderTargetView(
			pDevice,
			&rtvHandle
		);
		rtvHandle.Offset(1, m_rtvHandleIncSize);
	}
}

void Textures::CreateDescriptorHeaps(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const Type& type,
	size_t texturesCount
) {
	if (type == SRV || type == RTV_SRV) {
		m_pSRVDescHeap = CreateTextureDescHeap(
			pDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			texturesCount,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		);
	}
	if (type == RTV || type == RTV_SRV) {
		m_pRTVDescHeap = CreateTextureDescHeap(
			pDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			texturesCount
		);
	}

	m_pTextures.clear();
	m_pTextures.resize(texturesCount);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Textures::CreateTextureDescHeap(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_DESCRIPTOR_HEAP_TYPE& type,
	const UINT& texturesCount,
	const D3D12_DESCRIPTOR_HEAP_FLAGS& flags
) {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap{};
	D3D12_DESCRIPTOR_HEAP_DESC desc{ type, texturesCount, flags };
	ThrowIfFailed(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescHeap)));

	return pDescHeap;
}

UINT Textures::GetTexturesCount() const {
	return m_pSRVDescHeap->GetDesc().NumDescriptors;
}

std::shared_ptr<Texture> Textures::GetTexture(size_t textureId) const {
	return textureId < m_pTextures.size() ? m_pTextures[textureId] : nullptr;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Textures::GetSRVDescHeap() const {
	return m_pSRVDescHeap;
}
D3D12_CPU_DESCRIPTOR_HANDLE Textures::GetCpuSrvDescHandle(size_t textureId) const {
	return !m_pSRVDescHeap ? D3D12_CPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_pSRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		textureId,
		m_srvHandleIncSize
	);
}
D3D12_GPU_DESCRIPTOR_HANDLE Textures::GetGpuSrvDescHandle(size_t textureId) const {
	return !m_pSRVDescHeap ? D3D12_GPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_pSRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
		textureId,
		m_srvHandleIncSize
	);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Textures::GetRtvDescHeap() const {
	return m_pRTVDescHeap;
}
D3D12_CPU_DESCRIPTOR_HANDLE Textures::GetCpuRtvDescHandle(size_t textureId) const {
	return !m_pRTVDescHeap ? D3D12_CPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_pRTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		textureId,
		m_rtvHandleIncSize
	);
}
D3D12_GPU_DESCRIPTOR_HANDLE Textures::GetGpuRtvDescHandle(size_t textureId) const {
	return !m_pRTVDescHeap ? D3D12_GPU_DESCRIPTOR_HANDLE{ 0 } : CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_pRTVDescHeap->GetGPUDescriptorHandleForHeapStart(),
		textureId,
		m_rtvHandleIncSize
	);
}
