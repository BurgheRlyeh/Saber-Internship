#include "TextureResource.h"

#include "CommandList.h"
#include "Device.h"

TextureResource::TextureResource(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) : GPUResource(name, pDevice, heapData, resData, allocationFlags) {
	assert(
		resData.resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ||
		resData.resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resData.resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
	);
}

std::shared_ptr<TextureResource> TextureResource::FromSwapChain(
	Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
	size_t backBufferId
) {
	struct MakeSharedEnabler : public TextureResource {};
	std::shared_ptr<TextureResource> pTexRes{ std::make_shared<MakeSharedEnabler>() };

	ThrowIfFailed(pSwapChain->GetBuffer(backBufferId, IID_PPV_ARGS(&pTexRes->m_pResource)));
	pTexRes->m_pResource->SetName((L"BackBuffer" + std::to_wstring(backBufferId)).c_str());
	pTexRes->m_state = D3D12_RESOURCE_STATE_COMMON;

	return pTexRes;
}

bool TextureResource::IsDsv() const {
	return GetResource()->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
}
const D3D12_DEPTH_STENCIL_VIEW_DESC* TextureResource::GetDsvDesc() const {
	return nullptr;
}
void TextureResource::CreateDepthStencilView(
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc
) {
	assert(IsDsv());
	pDevice->GetD3D12Device()->CreateDepthStencilView(
		GetResource().Get(),
		pDsvDesc ? pDsvDesc : GetDsvDesc(),
		cpuDescHandle
	);
}

void TextureResource::ClearDepthTarget(
	std::shared_ptr<CommandList> pCommandList,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
	float depth,
	const D3D12_CLEAR_FLAGS& clearFlags,
	uint8_t stencil
) {
	assert(IsDsv());
	pCommandList->GetD3D12CommandList()->ClearDepthStencilView(
		cpuDescHandle,
		clearFlags,
		depth,
		stencil,
		0,
		nullptr
	);
}


