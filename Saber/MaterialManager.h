#pragma once

#include "Headers.h"

#include "Atlas.h"
#include "MaterialCB.h"

template <typename T>
class Buffer;
class CommandList;
class DDSTexture;
class DescriptorHeapManager;
class DescRange;
class Device;
class DeviceContext;
class TextureResource;

class MaterialManager {
	static const std::wstring BASE_NAME;

	std::shared_ptr<DescRange> m_pSRVsRange{};

	MaterialCB m_materialCB{};
	std::shared_ptr<Buffer<MaterialCB>> m_pMaterialCB{};

	struct RenderMaterial {
		std::shared_ptr<TextureResource> pAlbedo{};
		std::shared_ptr<TextureResource> pNormal{};
	};
	std::vector<std::shared_ptr<RenderMaterial>> m_pMaterials{};

	std::shared_ptr<Atlas<DDSTexture>> m_pTextureAtlas{};

public:
	MaterialManager(
		const std::wstring& resourceFolder,
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManager,
		const size_t& capacity
	);
	~MaterialManager();

	std::shared_ptr<DescRange> GetMaterialCBVsRange() const;
	std::shared_ptr<DescRange> GetMaterialSRVsRange() const;

	size_t AddMaterial(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect,
		const std::wstring& albedoFilepath,
		const std::wstring& normalFilepath
	);

private:
	size_t AddTexture(
		std::shared_ptr<Device> pDevice,
		std::shared_ptr<TextureResource> pTex
	);
};
