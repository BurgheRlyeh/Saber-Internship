#include "Textures.h"

Textures::Textures(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice)
	: m_handleIncrementSize(pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
{}

Textures::Textures(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	UINT texturesCount
) : Textures(pDevice) {
	CreateTextureDescHeap(pDevice, texturesCount);
}

Textures::Textures(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
	std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const LPCWSTR* filenames,
	size_t texturesCount
) : Textures(pDevice, static_cast<UINT>(pDevice, texturesCount)) {
	for (size_t i{}; i < GetTexturesCount(); ++i) {
		AddTexture(
			pDevice,
			pCommandQueueCopy,
			pCommandQueueDirect,
			pAllocator,
			filenames[i]
		);
	}
}

bool Textures::CreateTextureDescHeap(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	UINT texturesCount
) {
	if (m_isTextureDescHeapCreated.load()) {
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{
		.Type{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV },
		.NumDescriptors{ static_cast<UINT>(texturesCount) },
		.Flags{ D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE }
	};

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pTextureDescHeap)));
	m_CPUDescHandle = m_pTextureDescHeap->GetCPUDescriptorHandleForHeapStart();

	m_isTextureDescHeapCreated.store(true);
	return true;
}

bool Textures::AddTexture(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
	std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const LPCWSTR& filename,
	DirectX::DDS_FLAGS flags
) {
	if (!m_isTextureDescHeapCreated.load() || GetTexturesCount() <= m_pTextures.size()) {
		return false;
	}

	m_pTextures.push_back(std::make_shared<Texture>(
		pDevice,
		pCommandQueueCopy,
		pCommandQueueDirect,
		pAllocator,
		filename,
		&m_CPUDescHandle,
		flags
	));

	m_CPUDescHandle.Offset(1, m_handleIncrementSize);
	return false;
}

UINT Textures::GetTexturesCount() const {
	return m_pTextureDescHeap->GetDesc().NumDescriptors;
}

UINT Textures::GetHandleIncrementSize() const {
	return m_handleIncrementSize;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Textures::GetDescHeap() const {
	return m_pTextureDescHeap;
}