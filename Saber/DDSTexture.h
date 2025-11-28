#pragma once

#include "Headers.h"

#include "TextureResource.h"

class CommandList;
class DeviceContext;

class DDSTexture : public TextureResource {
protected:
	using TextureResource::TextureResource;

public:
	DDSTexture(
		const std::wstring& filename,
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	);

	void LoadFromDDS(
		const std::wstring& filename,
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandListDirect
	);
};