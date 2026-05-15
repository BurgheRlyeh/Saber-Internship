/**
 * @file TextureResource.h
 * @brief GPU texture resource wrapper derived from @ref GPUResource.
 *
 * @ref TextureResource extends @ref GPUResource with a factory method for
 * wrapping swap-chain back buffers (@ref FromSwapChain) and leaves room for
 * future DSV support (currently commented out).
 *
 * The class relies on @ref GPUResource for all view creation, resource
 * transition, and allocation logic.  It is used for render targets, G-buffer
 * slices, DDS textures, and swap-chain back buffers.
 */
#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Device;

/**
 * @brief Thin @ref GPUResource subclass for 2-D texture resources.
 *
 * Inherits the @ref GPUResource constructor family via @c using to allow
 * construction from any valid @ref AllocationDesc / @ref ResourceDesc pair.
 */
class TextureResource : public GPUResource {
protected:
	using GPUResource::GPUResource;

public:
	/**
	 * @brief Constructs a texture resource with explicit allocation and resource descriptors.
	 * @param name      Debug name.
	 * @param pDevice   D3D12 device wrapper.
	 * @param allocDesc D3D12MA allocation descriptor (heap type, flags).
	 * @param resDesc   D3D12 resource descriptor and initial state.
	 */
	TextureResource(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const AllocationDesc& allocDesc,
		const ResourceDesc& resDesc
	);

	/**
	 * @brief Wraps a swap-chain back buffer in a @ref TextureResource without allocating new memory.
	 * @param pSwapChain   The DXGI swap chain.
	 * @param backBufferId Zero-based back-buffer index.
	 * @return Shared pointer to the wrapped @ref TextureResource.
	 */
	static std::shared_ptr<TextureResource> FromSwapChain(
		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
		size_t backBufferId
	);
};
