#pragma once

#include "Headers.h"

#include "DirectXTex.h"

#include "CommandQueue.h"
#include "TextureResource.h"

class DDSTexture : public TextureResource {
public:
	using TextureResource::TextureResource;
	DDSTexture(
		const std::wstring& filename,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	);

	void LoadFromDDS(
		const std::wstring& filename,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	);
};