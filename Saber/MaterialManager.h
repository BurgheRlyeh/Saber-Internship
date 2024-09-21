#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "ConstantBuffer.h"
#include "DDSTexture.h"
#include "DescriptorHeapManager.h"
#include "Texture.h"

class MaterialManager {
	struct MaterialCB {
		DirectX::XMUINT4 texsId{};

		MaterialCB(size_t albedoId = 0, size_t normalId = 0) : texsId{
			static_cast<UINT>(albedoId), static_cast<UINT>(normalId), 0, 0
		} {}
	};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescHeap{};
	std::shared_ptr<DescHeapRange> m_pSRVsRange{};
	std::shared_ptr<DescHeapRange> m_pCBVsRange{};

	struct RenderMaterial {
		std::shared_ptr<Texture> pAlbedo{};
		std::shared_ptr<Texture> pNormal{};
		std::shared_ptr<ConstantBuffer> pCB{};
	};
	std::vector<std::shared_ptr<RenderMaterial>> m_pMaterials{};

	std::shared_ptr<Atlas<DDSTexture>> m_pTextureAtlas{};

public:
	MaterialManager(
		const std::wstring& resourceFolder,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManager,
		const size_t& capacity
	);
	~MaterialManager();

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescHeap() const {
		return m_pDescHeap;
	}
	std::shared_ptr<DescHeapRange> GetMaterialCBVsRange() const;
	std::shared_ptr<DescHeapRange> GetMaterialSRVsRange() const;

	size_t AddMaterial(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect,
		const std::wstring& albedoFilepath,
		const std::wstring& normalFilepath
	);

private:
	size_t AddTexture(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<Texture> pTex);
	size_t AddConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<ConstantBuffer> pBuf);
};
