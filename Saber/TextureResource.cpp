#include "TextureResource.h"

bool TextureResource::IsDsv() const {
	return GetResource()->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
}
const D3D12_DEPTH_STENCIL_VIEW_DESC* TextureResource::GetDsvDesc() const {
	return nullptr;
}
void TextureResource::CreateDepthStencilView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc
) {
	assert(IsDsv());
	pDevice->CreateDepthStencilView(
		GetResource().Get(),
		pDsvDesc ? pDsvDesc : GetDsvDesc(),
		cpuDescHandle
	);
}


