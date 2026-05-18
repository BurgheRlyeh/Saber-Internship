#include "MaterialManager.h"

#include "Buffer.h"
#include "CommandList.h"
#include "DDSTexture.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "Device.h"
#include "DeviceContext.h"
#include "TextureResource.h"

TextureManager::TextureManager(
	const std::wstring& name,
	const std::wstring& resourceFolder,
	std::shared_ptr<DescriptorHeap> pDescHeapManager,
	size_t capacity
) : m_name(name), m_resourceFolder(resourceFolder) {
	m_pSrvRange = pDescHeapManager->AllocateRange(m_name, DescRangeType::Srv, capacity);
}

std::shared_ptr<DescRange> TextureManager::GetSrvRange() const {
	return m_pSrvRange;
}

size_t TextureManager::GetCreateTextureId(
	const std::wstring& filename,
	std::shared_ptr<DeviceContext> pDeviceContext,
	std::shared_ptr<CommandList> pCommandList
) {
    std::unique_lock lock(m_textureIdMapMutex);
	if (auto it = m_textureIdMap.find(filename); it != m_textureIdMap.end())
		return it->second;

    size_t srvId{ m_pSrvRange->Allocate() };
    m_textureIdMap[filename] = srvId;
    lock.unlock();

    auto pTex{ std::make_shared<DDSTexture>(m_resourceFolder + filename, pDeviceContext, pCommandList) };
	m_pTextures.push_back(pTex);

    D3D12_CPU_DESCRIPTOR_HANDLE handle{ m_pSrvRange->GetCpuHandle(srvId) };
    pTex->CreateShaderResourceView(pDeviceContext->GetDevice(), handle);

    return srvId;
}

const std::wstring MaterialManager::BASE_NAME = L"MaterialManager";

MaterialManager::MaterialManager(
    const std::wstring& resourceFolder,
    std::shared_ptr<DeviceContext> pDeviceContext,
    size_t capacity
) : m_capacity(capacity) {
    m_pTexManager = std::make_shared<TextureManager>(
        BASE_NAME + L"/TextureManager",
        resourceFolder,
        pDeviceContext->GetDescriptorHeap(DescRangeType::Srv),
        2 * capacity        // (albedo + normal) * count
    );

    m_pMaterialBuffer = CreateUploadBufferWithUpdater<MaterialCB>(
        BASE_NAME + L"/MaterialCB",
        pDeviceContext,
        ResourceView::Cbv
    );
    m_materialBuffer.materials[0] = { 0, 0, 0, 0 };
    m_pMaterialBuffer->UpdateAll(&m_materialBuffer, 1);
}

std::shared_ptr<DescRange> MaterialManager::GetMaterialCbvRange() const {
    return m_pMaterialBuffer->GetDescRange(DescRangeType::Cbv);
}

std::shared_ptr<DescRange> MaterialManager::GetMaterialSrvRange() const {
    return m_pTexManager->GetSrvRange();
}

size_t MaterialManager::GetCreateMaterial(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList,
    const std::wstring& albedoFilepath,
    const std::wstring& normalFilepath
) {
    MaterialKey key{ albedoFilepath, normalFilepath };

    std::unique_lock matMapLock(m_materialIdMapMutex);
    if (auto it = m_materialIdMap.find(key); it != m_materialIdMap.end())
        return it->second;

    size_t materialId = m_materialIdMap.size() + 1;
    if (materialId >= m_capacity)
        throw std::runtime_error("Material limit exceeded");

    m_materialIdMap[key] = materialId;
    matMapLock.unlock();

    m_materialBuffer.materials[materialId] = {
        static_cast<UINT>(m_pTexManager->GetCreateTextureId(albedoFilepath, pDeviceContext, pCommandList)),
        static_cast<UINT>(m_pTexManager->GetCreateTextureId(normalFilepath, pDeviceContext, pCommandList)),
        0,
        0
    };
    m_pMaterialBuffer->UpdateAll(&m_materialBuffer, 1);

    return materialId;
}
