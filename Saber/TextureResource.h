#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Device;

class TextureResource : public GPUResource {
protected:
	using GPUResource::GPUResource;

public:
	TextureResource(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const AllocationDesc& allocDesc,
		const ResourceDesc& resDesc
	);

	static std::shared_ptr<TextureResource> FromSwapChain(
		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
		size_t backBufferId
	);

	size_t GetWidth() const {
		return GetResourceDesc().Width;
	}

	size_t GetHeight() const {
		return GetResourceDesc().Height;
	}
};
