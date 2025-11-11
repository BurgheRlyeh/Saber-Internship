#include "MaterialManager.h"

#include "CommandQueue.h"
#include "ConstantBuffer.h"
#include "DDSTexture.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "Device.h"
#include "DeviceContext.h"
#include "TextureResource.h"

const std::wstring MaterialManager::BASE_NAME = L"MaterialManager";

MaterialManager::MaterialManager(
	const std::wstring& resourceFolder,
	std::shared_ptr<Device> pDevice,
	std::shared_ptr<DescriptorHeapManager> pDescHeapManager,
	const size_t& capacity
) {
	m_pTextureAtlas = std::make_shared<Atlas<DDSTexture>>(resourceFolder);

	m_pCBVsRange = pDescHeapManager->AllocateRange(
		BASE_NAME + L"/Ranges/Cbv",
		1,
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV
	);
	m_pSRVsRange = pDescHeapManager->AllocateRange(
		BASE_NAME + L"/Ranges/Srv",
		2 * capacity,	// (albedo + normal) * count
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV
	);

	m_pMaterialCB = std::make_shared<ConstantBuffer>(
		BASE_NAME + L"/MaterialCB",
		pDevice,
		sizeof(MaterialCB),
		&m_materialCB
	);
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

size_t MaterialManager::AddMaterial(
	std::shared_ptr<DeviceContext> pDeviceContext,
	std::shared_ptr<CommandQueue> pCommandQueueDirect,
	const std::wstring& albedoFilepath,
	const std::wstring& normalFilepath
) {
	std::shared_ptr<TextureResource> pAlbedo{
		m_pTextureAtlas->Assign(albedoFilepath, pDeviceContext, pCommandQueueDirect)
	};
	std::shared_ptr<DDSTexture> pNormal{
		m_pTextureAtlas->Assign(normalFilepath, pDeviceContext, pCommandQueueDirect)
	};
	m_pMaterials.push_back(std::make_shared<RenderMaterial>(pAlbedo, pNormal));

	size_t materialId{ m_pMaterials.size() - 1 };
	m_materialCB.materials[materialId] = {
		static_cast<UINT>(AddTexture(pDeviceContext->GetDevice(), pAlbedo)),
		static_cast<UINT>(AddTexture(pDeviceContext->GetDevice(), pNormal)),
		0,
		0
	};
	m_pMaterialCB->Update(&m_materialCB);

	return materialId;
}

size_t MaterialManager::AddTexture(
	std::shared_ptr<Device> pDevice,
	std::shared_ptr<TextureResource> pTex
) {
	size_t id{ m_pSRVsRange->GetNextId() };

	auto handle = m_pSRVsRange->GetCpuHandle(id);
	pTex->CreateShaderResourceView(pDevice, handle);

	return id;
}
