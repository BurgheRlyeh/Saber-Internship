#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Texture : public GPUResource {
public:
	using GPUResource::GPUResource;

	bool IsDsv() const;
	virtual const D3D12_DEPTH_STENCIL_VIEW_DESC* GetDsvDesc() const;;
	void CreateDepthStencilView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc = nullptr
	);
};
