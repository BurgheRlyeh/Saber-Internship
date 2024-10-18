#include "Texture.h"

bool Texture::IsDsv() const {
	return m_flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
}
const D3D12_DEPTH_STENCIL_VIEW_DESC* Texture::GetDsvDesc() const {
	return nullptr;
}
void Texture::CreateDepthStencilView(
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


