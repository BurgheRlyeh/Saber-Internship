#pragma once

#include "Headers.h"

#include "DirectXTex.h"

#include "CommandQueue.h"
#include "Texture.h"

class DDSTexture : public Texture {
	DirectX::TexMetadata m_metadata{};
	std::unique_ptr<D3D12_SHADER_RESOURCE_VIEW_DESC> m_pSrvDesc{};

public:
	DDSTexture(
		const std::wstring& filename,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	);

	const D3D12_SHADER_RESOURCE_VIEW_DESC* GetSrvDesc() const override;

	using GPUResource::GetResource;
	using Texture::CreateShaderResourceView;

private:
	void LoadFromDDS(
		const std::wstring& filename,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	);

	D3D12_SHADER_RESOURCE_VIEW_DESC CreateSrvDesc() const;
};