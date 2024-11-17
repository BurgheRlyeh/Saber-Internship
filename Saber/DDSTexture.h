#pragma once

#include "Headers.h"

#include "DirectXTex.h"

#include "CommandQueue.h"
#include "Texture.h"

class DDSTexture : public Texture {
public:
	using Texture::Texture;
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