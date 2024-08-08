#pragma once

#include "Headers.h"

#include "DirectXTex.h"

#include "GPUResource.h"
#include "CommandQueue.h"

class Texture : public GPUResource {
	DirectX::ScratchImage m_image{};
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pTextureDescHeap{};

public:
	Texture(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
		, std::shared_ptr<CommandQueue> const& pCommandQueueCopy
		, std::shared_ptr<CommandQueue> const& pCommandQueueDirect
		, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
		, const wchar_t* filename
		, DirectX::DDS_FLAGS flags = DirectX::DDS_FLAGS_NONE
	);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetTextureDescHeap() const;
};