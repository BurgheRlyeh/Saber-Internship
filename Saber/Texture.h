#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Texture : public GPUResource {
public:
	using GPUResource::GPUResource;

	Texture(
		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
		size_t backBufferId
	) {
		ThrowIfFailed(pSwapChain->GetBuffer(backBufferId, IID_PPV_ARGS(&m_pResource)));
		m_state = D3D12_RESOURCE_STATE_COMMON;
	}

	bool IsDsv() const;
	virtual const D3D12_DEPTH_STENCIL_VIEW_DESC* GetDsvDesc() const;
	void CreateDepthStencilView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
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
