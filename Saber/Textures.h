#pragma once

#include "Headers.h"

#include <atomic>
#include <vector>

#include "Texture.h"

class Textures {
	std::vector<std::shared_ptr<Texture>> m_pTextures{};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pTextureDescHeap{};
	std::atomic<bool> m_isTextureDescHeapCreated{};

	CD3DX12_CPU_DESCRIPTOR_HANDLE m_CPUDescHandle{};
	UINT m_handleIncrementSize;

public:
	Textures() = delete;
	Textures(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice);

	Textures(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		UINT texturesCount
	);

	Textures(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const LPCWSTR* filenames,
		size_t texturesCount
	);

	bool CreateTextureDescHeap(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		UINT texturesCount
	);

	bool AddTexture(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const LPCWSTR& filename,
		DirectX::DDS_FLAGS flags = DirectX::DDS_FLAGS_NONE
	);

	UINT GetTexturesCount() const;

	UINT GetHandleIncrementSize() const;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescHeap() const;
};