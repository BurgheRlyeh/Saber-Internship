#pragma once

#include "Headers.h"

#include <atomic>
#include <vector>

#include "CommandQueue.h"
#include "DDSTexture.h"
#include "Texture.h"

class Textures {
	std::vector<std::shared_ptr<Texture>> m_pTextures{};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pSrvUavDescHeap{};
	const UINT m_srvUavHandleIncSize;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRTVDescHeap{};
	const UINT m_rtvHandleIncSize;

//public:
//	struct TextureId {
//		size_t srvUavId{ static_cast<size_t>(-1) };
//		size_t rtvId{ static_cast<size_t>(-1) };
//	};
//
//private:
//	TextureId m_resourcesCounter{};

public:
	Textures() = delete;
	Textures(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice);
	Textures(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		size_t texturesCount
	);

	//TextureId AddTexture(
	//	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	//	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	//	const D3D12_RESOURCE_DESC& resDesc,
	//	const D3D12_HEAP_TYPE& heapType = D3D12_HEAP_TYPE_DEFAULT,
	//	const D3D12_RESOURCE_STATES& resState = D3D12_RESOURCE_STATE_COMMON,
	//	const D3D12_CLEAR_VALUE* pClearValue = nullptr
	//) {
	//	TextureId texId{};

	//	m_pTextures.push_back(std::make_shared<Texture>(
	//		pAllocator,
	//		resDesc,
	//		heapType,
	//		resState,
	//		pClearValue
	//	));

	//	if (resDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
	//		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{
	//			GetCpuRtvDescHandle(m_resourcesCounter.rtvId)
	//		};
	//		m_pTextures.back()->CreateRenderTargetView(pDevice, rtvHandle);
	//		texId.rtvId = m_resourcesCounter.rtvId++;
	//	}

	//	bool isSrvUav{};
	//	CD3DX12_CPU_DESCRIPTOR_HANDLE srvUavHandle{ 
	//		GetCpuSrvUavDescHandle(m_resourcesCounter.srvUavId)
	//	};
	//	if (!(resDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
	//		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
	//			.Format{ resDesc.Format }, 
	//			.ViewDimension{ ResToSrvDim(resDesc.Dimension) },
	//			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
	//			.Texture2D{
	//				.MipLevels{ 1 }
	//			}
	//		};
	//		m_pTextures.back()->CreateShaderResourceView(
	//			pDevice,
	//			srvUavHandle,
	//			&srvDesc
	//		);
	//		isSrvUav = true;
	//	}
	//	if (resDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
	//		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
	//			.Format{ resDesc.Format },
	//			.ViewDimension{ ResToUavDim(resDesc.Dimension) }
	//		};
	//		m_pTextures.back()->CreateUnorderedAccessView(
	//			pDevice,
	//			srvUavHandle,
	//			&uavDesc
	//		);
	//		isSrvUav = true;
	//	}
	//	if (isSrvUav) {
	//		texId.srvUavId = m_resourcesCounter.srvUavId++;
	//	}

	//	return texId;
	//}

	static std::shared_ptr<Textures> LoadSRVsFromDDS(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const std::wstring* filenames,
		size_t texturesCount
	) {
		std::shared_ptr<Textures> pTexs{ std::make_shared<Textures>(
			pDevice, texturesCount
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

	static std::shared_ptr<Textures> CreateTexturePack(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_DESC* resourceDesc,
		size_t texturesCount,
		const D3D12_CLEAR_VALUE* pClearValue = nullptr
	) {
		std::shared_ptr<Textures> pTexs{ std::make_shared<Textures>(
			pDevice, texturesCount
		) };
		pTexs->Resize(
			pDevice,
			pAllocator,
			resourceDesc,
			texturesCount,
			pClearValue
		);

		return pTexs;
	}

	//static std::shared_ptr<Textures> CreateBindless(

	//)

	void LoadFromDDS(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const std::wstring* filenames,
		size_t texturesCount
	);

	void Resize(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_DESC* resourceDesc,
		size_t texturesCount,
		const D3D12_CLEAR_VALUE* pClearValue = nullptr
	);

	void CreateDescriptorHeaps(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		size_t texturesCount
	);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateTextureDescHeap(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_DESCRIPTOR_HEAP_TYPE& type,
		const size_t& texturesCount,
		const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	UINT GetTexturesCount() const;

	std::shared_ptr<Texture> GetTexture(size_t textureId) const;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvUavDescHeap() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrvUavDescHandle(size_t textureId = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrvUavDescHandle(size_t textureId = 0) const;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRtvDescHeap() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuRtvDescHandle(size_t textureId = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuRtvDescHandle(size_t textureId = 0) const;
};