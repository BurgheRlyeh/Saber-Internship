#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "ConstantBuffer.h"
#include "DDSTexture.h"
#include "DescriptorHeapManager.h"
#include "Texture.h"

#include "MaterialCB.h"

class MaterialManager {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescHeap{};
	std::shared_ptr<DescHeapRange> m_pCBVsRange{};
	std::shared_ptr<DescHeapRange> m_pSRVsRange{};

	MaterialCB m_materialCB{};
	std::shared_ptr<ConstantBuffer> m_pMaterialCB{};

	struct RenderMaterial {
		std::shared_ptr<Texture> pAlbedo{};
		std::shared_ptr<Texture> pNormal{};
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
};
