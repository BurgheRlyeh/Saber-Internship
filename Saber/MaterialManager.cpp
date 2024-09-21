#include "MaterialManager.h"

MaterialManager::MaterialManager(
	const std::wstring& resourceFolder,
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<DescriptorHeapManager> pDescHeapManager,
	const size_t& capacity
) {
	m_pTextureAtlas = std::make_shared<Atlas<DDSTexture>>(resourceFolder);
	
	m_pDescHeap = pDescHeapManager->GetDescriptorHeap();
	m_pSRVsRange = pDescHeapManager->AllocateRange(
		L"Materials/SRVs",
		2 * capacity,	// (albedo + normal) * count
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV
	);
	m_pCBVsRange = pDescHeapManager->AllocateRange(
		L"Materials/CBVs",
		1 + capacity,	// emptyMaterial + count
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV
	);

	m_pMaterials.reserve(capacity);

	// create empty material
	MaterialCB emptyMaterial{};
	std::shared_ptr<ConstantBuffer> pEmptyMaterial{
		std::make_shared<ConstantBuffer>(pAllocator, sizeof(emptyMaterial), &emptyMaterial)
	};
	m_pMaterials.push_back(std::make_shared<RenderMaterial>(nullptr, nullptr, pEmptyMaterial));
	AddConstantBuffer(pDevice, pEmptyMaterial);
}

MaterialManager::~MaterialManager() {
	m_pMaterials.clear();
}

std::shared_ptr<DescHeapRange> MaterialManager::GetMaterialCBVsRange() const {
	return m_pCBVsRange;
}

std::shared_ptr<DescHeapRange> MaterialManager::GetMaterialSRVsRange() const {
	return m_pSRVsRange;
}

size_t MaterialManager::AddMaterial(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator, std::shared_ptr<CommandQueue> pCommandQueueCopy, std::shared_ptr<CommandQueue> pCommandQueueDirect, const std::wstring& albedoFilepath, const std::wstring& normalFilepath) {
	std::shared_ptr<Texture> pAlbedo{
		m_pTextureAtlas->Assign(albedoFilepath, pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect)
	};
	std::shared_ptr<DDSTexture> pNormal{
		m_pTextureAtlas->Assign(normalFilepath, pDevice, pAllocator, pCommandQueueCopy, pCommandQueueDirect)
	};
	MaterialCB materialCB{ AddTexture(pDevice, pAlbedo), AddTexture(pDevice, pNormal) };
	std::shared_ptr<ConstantBuffer> pCB{
		std::make_shared<ConstantBuffer>(pAllocator, sizeof(materialCB), &materialCB)
	};
	m_pMaterials.push_back(std::make_shared<RenderMaterial>(pAlbedo, pNormal, pCB));

	return AddConstantBuffer(pDevice, pCB);
}

size_t MaterialManager::AddTexture(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<Texture> pTex) {
	size_t id{ m_pSRVsRange->GetNextId() };

	auto handle = m_pSRVsRange->GetCpuHandle(id);
	pTex->CreateShaderResourceView(pDevice, handle);

	return id;
}

size_t MaterialManager::AddConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<ConstantBuffer> pBuf) {
	size_t id{ m_pCBVsRange->GetNextId() };
	pBuf->CreateConstantBufferView(pDevice, m_pCBVsRange->GetCpuHandle(id));
	return id;
}
