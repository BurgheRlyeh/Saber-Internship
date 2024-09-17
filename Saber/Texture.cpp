#include "Texture.h"

void Texture::CreateRenderTargetView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc
) {
	//assert(IsRtv());
	//assert(!pRtvDesc || pRtvDesc->Format == m_format);
	if (!IsRtv()) {
		return;
	}
	pDevice->CreateRenderTargetView(
		GetResource().Get(),
		pRtvDesc,
		cpuDescHandle
	);
}

void Texture::CreateShaderResourceView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc
) {
	//assert(IsSrv());
	//assert(!pSrvDesc || pSrvDesc->Format == m_format);
	if (!IsSrv()) {
		return;
	}
	pDevice->CreateShaderResourceView(
		GetResource().Get(),
		pSrvDesc,
		cpuDescHandle
	);
}

void Texture::CreateUnorderedAccessView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc,
	Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource
) {
	//assert(IsUav());
	//assert(!pUavDesc || pUavDesc->Format == m_format);
	if (!IsUav()) {
		return;
	}
	pDevice->CreateUnorderedAccessView(
		GetResource().Get(),
		pCounterResource.Get(),
		pUavDesc,
		cpuDescHandle
	);
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

