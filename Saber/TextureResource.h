#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Device;

class TextureResource : public GPUResource {
public:
	using GPUResource::GPUResource;

	static std::shared_ptr<TextureResource> FromSwapChain(
		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
		size_t backBufferId
	) {
		std::shared_ptr<TextureResource> pTexRes{ std::make_shared<TextureResource>() };
		
		ThrowIfFailed(pSwapChain->GetBuffer(backBufferId, IID_PPV_ARGS(&pTexRes->m_pResource)));
		pTexRes->m_pResource->SetName((L"BackBuffer" + std::to_wstring(backBufferId)).c_str());
		
		pTexRes->m_state = D3D12_RESOURCE_STATE_COMMON;
		
		return pTexRes;
	}

	bool IsDsv() const;
	virtual const D3D12_DEPTH_STENCIL_VIEW_DESC* GetDsvDesc() const;
	void CreateDepthStencilView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc = nullptr
	);

	void ClearDepthTarget(
		std::shared_ptr<CommandList> pCommandList,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle
	) {
		assert(IsDsv());

		pCommandList->GetD3D12CommandList()->ClearDepthStencilView(
			cpuDescHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			0.f,
			0,
			0,
			nullptr
		);
	}
};
