#pragma once

#include "Headers.h"

#include <atomic>
#include <vector>

#include "Texture.h"

class Textures {
	std::vector<std::shared_ptr<Texture>> m_pTextures{};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pSRVDescHeap{};
	const UINT m_srvHandleIncSize;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRTVDescHeap{};
	const UINT m_rtvHandleIncSize;

public:
	Textures() = delete;
	Textures(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice);

	enum Type{ SRV, RTV, RTV_SRV };
	Textures(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const Type& type,
		size_t texturesCount
	);

	static std::shared_ptr<Textures> LoadSRVsFromDDS(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const LPCWSTR* filenames,
		size_t texturesCount
	) {
		std::shared_ptr<Textures> pTexs{ std::make_shared<Textures>(
			pDevice, SRV, texturesCount
		) };
		pTexs->LoadFromDDS(
			pDevice,
			pCommandQueueCopy,
			pCommandQueueDirect,
			pAllocator,
			filenames,
			texturesCount
		);

		return pTexs;
	}

	static std::shared_ptr<Textures> CreateRTVs(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		size_t texturesCount,
		const D3D12_RESOURCE_DESC& resourceDesc,
		const D3D12_CLEAR_VALUE* pClearValue = nullptr,
		bool isShaderAvailable = true
	) {
		std::shared_ptr<Textures> pTexs{ std::make_shared<Textures>(
			pDevice, RTV_SRV, texturesCount
		) };
		pTexs->Resize(
			pDevice,
			pAllocator,
			resourceDesc,
			pClearValue
		);

		return pTexs;
	}

	void LoadFromDDS(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const LPCWSTR* filenames,
		size_t texturesCount
	);

	void Resize(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_DESC& resourceDesc,
		const D3D12_CLEAR_VALUE* pClearValue = nullptr
	);

	void CreateDescriptorHeaps(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const Type& type,
		size_t texturesCount
	);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateTextureDescHeap(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_DESCRIPTOR_HEAP_TYPE& type,
		const UINT& texturesCount,
		const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	UINT GetTexturesCount() const;

	std::shared_ptr<Texture> GetTexture(size_t textureId) const;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSRVDescHeap() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrvDescHandle(size_t textureId = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrvDescHandle(size_t textureId = 0) const;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRtvDescHeap() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuRtvDescHandle(size_t textureId = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuRtvDescHandle(size_t textureId = 0) const;
};