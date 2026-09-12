#pragma once

#include "Headers.h"

#include <unordered_map>
#include <mutex>

#include "MaterialCB.h"

template <typename T>
class Buffer;
class CommandList;
class DDSTexture;
class DescriptorHeap;
class DescRange;
class Device;
class DeviceContext;
class TextureResource;

class TextureManager {
	std::wstring m_name{};

	std::wstring m_resourceFolder{};

	std::vector<std::shared_ptr<TextureResource>> m_pTextures{};
	std::shared_ptr<DescRange> m_pSrvRange{};

	std::unordered_map<std::wstring, size_t> m_textureIdMap{};
    std::mutex m_textureIdMapMutex;

public:
	TextureManager(
		const std::wstring& name,
		const std::wstring& resourceFolder,
		std::shared_ptr<DescriptorHeap> heap,
		size_t capacity
	);

	std::shared_ptr<DescRange> GetSrvRange() const;

	size_t GetCreateTextureId(
		const std::wstring& filepath,
		std::shared_ptr<DeviceContext> context,
		std::shared_ptr<CommandList> cmd
	);
};

class MaterialManager {
    static const std::wstring BASE_NAME;

    std::shared_ptr<TextureManager> m_pTexManager;

    struct MaterialKey {
        std::wstring albedo;
        std::wstring normal;
        PhongParams phong;

        bool operator==(const MaterialKey& other) const = default;

        struct Hasher {
            size_t operator()(const MaterialKey& k) const {
                size_t hash{ std::hash<std::wstring>()(k.albedo) };
                hash ^= std::hash<std::wstring>()(k.normal) << 1;

                for (const float& value : {
                    k.phong.ambient, k.phong.diffuse, k.phong.specular, k.phong.shininess
                }) {
                    hash ^= std::hash<float>()(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                }

                return hash;
            }
        };
    };
    std::unordered_map<MaterialKey, size_t, MaterialKey::Hasher> m_materialIdMap;
    std::mutex m_materialIdMapMutex;

    MaterialCB m_materialBuffer{};
    std::shared_ptr<Buffer<MaterialCB>> m_pMaterialBuffer;

    size_t m_capacity{};

public:
    MaterialManager(
        const std::wstring& resourceFolder,
        std::shared_ptr<DeviceContext> pDeviceContext,
        size_t capacity
    );

    std::shared_ptr<DescRange> GetMaterialCbvRange() const;
    std::shared_ptr<DescRange> GetMaterialSrvRange() const;

    size_t GetCreateMaterial(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList,
        const std::wstring& albedoFilepath,
        const std::wstring& normalFilepath,
        const PhongParams& phong = {}
    );
};
