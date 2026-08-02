#include "TextureResource.h"

#include "CommandList.h"
#include "Device.h"

TextureResource::TextureResource(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const AllocationDesc& allocDesc,
	const ResourceDesc& resDesc
) : GPUResource(name, pDevice, allocDesc, resDesc) {
	assert(
		resDesc.resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ||
		resDesc.resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resDesc.resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
	);
}

std::shared_ptr<TextureResource> TextureResource::FromSwapChain(
	Microsoft::WRL::ComPtr<DXGISwapChain> pSwapChain,
	size_t backBufferId
) {
	struct MakeSharedEnabler : public TextureResource {};
	std::shared_ptr<TextureResource> pTexRes{ std::make_shared<MakeSharedEnabler>() };

	ThrowIfFailed(pSwapChain->GetBuffer(backBufferId, IID_PPV_ARGS(&pTexRes->m_pResource)));
	pTexRes->m_pResource->SetName((L"BackBuffer" + std::to_wstring(backBufferId)).c_str());
	pTexRes->m_state = D3D12_RESOURCE_STATE_COMMON;

	return pTexRes;
}
