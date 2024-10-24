#include "Texture.h"

void Texture::CreateRenderTargetView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc
) {
	assert(IsRtv());
	pDevice->CreateRenderTargetView(
		GetResource().Get(),
		pRtvDesc ? pRtvDesc : GetRtvDesc(),
		cpuDescHandle
	);
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

void Texture::CreateShaderResourceView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc
) {
	assert(IsSrv());
	pDevice->CreateShaderResourceView(
		GetResource().Get(),
		pSrvDesc ? pSrvDesc : GetSrvDesc(),
		cpuDescHandle
	);
}

void Texture::CreateUnorderedAccessView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc,
	Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource
) {
	assert(IsUav());
	pDevice->CreateUnorderedAccessView(
		GetResource().Get(),
		pCounterResource.Get(),
		pUavDesc ? pUavDesc : GetUavDesc(),
		cpuDescHandle
	);
}

bool Texture::IsDsv() const {
	return m_flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
}

bool Texture::IsRtv() const {
	return m_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
}

bool Texture::IsSrv() const {
	return !(m_flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
}

bool Texture::IsUav() const {
	return m_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
}

