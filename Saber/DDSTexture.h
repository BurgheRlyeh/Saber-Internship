#pragma once

#include "Headers.h"

#include "TextureResource.h"

class CommandQueue;
class DeviceContext;

class DDSTexture : public TextureResource {
public:
	using TextureResource::TextureResource;
	DDSTexture(
		const std::wstring& filename,
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	);

	void LoadFromDDS(
		const std::wstring& filename,
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	);
};