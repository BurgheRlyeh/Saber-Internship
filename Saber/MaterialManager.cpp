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

	m_pCBVsRange = pDescHeapManager->AllocateRange(
		L"Materials/CBV",
		1,
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV
	);
	m_pSRVsRange = pDescHeapManager->AllocateRange(
		L"Materials/SRVs",
		2 * capacity,	// (albedo + normal) * count
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV
	);

	m_pMaterialCB = std::make_shared<ConstantBuffer>(pAllocator, sizeof(MaterialCB), &m_materialCB);
	m_pMaterialCB->CreateConstantBufferView(pDevice, m_pCBVsRange->GetNextCpuHandle());

	m_pMaterials.reserve(capacity);

	// empty material
	m_pMaterials.push_back(std::make_shared<RenderMaterial>(nullptr, nullptr));
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
	m_pMaterials.push_back(std::make_shared<RenderMaterial>(pAlbedo, pNormal));

	size_t materialId{ m_pMaterials.size() };
	m_materialCB.materials[materialId] = {
		static_cast<UINT>(AddTexture(pDevice, pAlbedo)),
		static_cast<UINT>(AddTexture(pDevice, pNormal)),
		0,
		0
	};
	m_pMaterialCB->Update(&m_materialCB);

	return materialId;
}

size_t MaterialManager::AddTexture(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<Texture> pTex) {
	size_t id{ m_pSRVsRange->GetNextId() };

	auto handle = m_pSRVsRange->GetCpuHandle(id);
	pTex->CreateShaderResourceView(pDevice, handle);

	return id;
}
