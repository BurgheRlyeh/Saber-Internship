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
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	static std::shared_ptr<TextureResource> FromSwapChain(
		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
		size_t backBufferId
	);

	bool IsDsv() const;
	virtual const D3D12_DEPTH_STENCIL_VIEW_DESC* GetDsvDesc() const;
	void CreateDepthStencilView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc = nullptr
	);

	void ClearDepthTarget(
		std::shared_ptr<CommandList> pCommandList,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
		float depth = 0.f,
		const D3D12_CLEAR_FLAGS& clearFlags = D3D12_CLEAR_FLAG_DEPTH,
		uint8_t stencil = 0
	);
};
