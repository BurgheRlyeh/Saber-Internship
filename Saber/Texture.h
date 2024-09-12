#pragma once

#include "Headers.h"

#include "DirectXTex.h"

#include "GPUResource.h"
#include "CommandQueue.h"

class Texture : public GPUResource {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pTextureDescHeap{};

public:
	Texture() = delete;
	using GPUResource::GPUResource;
	Texture(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
		std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const LPCWSTR& filename,
		D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHeap = nullptr,
		DirectX::DDS_FLAGS flags = DirectX::DDS_FLAGS_NONE
	);

	static std::shared_ptr<Texture> CreateRenderTarget(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_DESC& resourceDesc,
		D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHeap = nullptr,
		const D3D12_CLEAR_VALUE* pClearValue = nullptr
	) {
		std::shared_ptr<Texture> pTexture = std::make_shared<Texture>(
			pAllocator,
			resourceDesc,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			pClearValue
		);

		pTexture->CreateRenderTargetView(pDevice, pCPUDescHeap);
		return pTexture;
	}

	void CreateRenderTargetView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle = nullptr,
		const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
	void CreateShaderResourceView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, 
		const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc,
		const D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle = nullptr
	);
	void CreateUnorderedAccessView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc,
		const D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle = nullptr
	);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetTextureDescHeap() const;

private:
	void CreateDescriptorHeap(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_DESCRIPTOR_HEAP_TYPE& type,
		const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);

	void InitSRVFromDDSFile(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		std::shared_ptr<CommandQueue> const& pCommandQueueCopy,
		std::shared_ptr<CommandQueue> const& pCommandQueueDirect,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const LPCWSTR& filename,
		D3D12_CPU_DESCRIPTOR_HANDLE* pCPUDescHandle = nullptr,
		DirectX::DDS_FLAGS flags = DirectX::DDS_FLAGS_NONE
	);
};

static void ClearRenderTarget(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
	const float* clearColor = nullptr
) {
	assert(pBuffer->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	if (!clearColor) {
		static float defaultColor[] = {
			.4f,
			.6f,
			.9f,
			1.f
		};

		clearColor = defaultColor;
	}

	pCommandList->ClearRenderTargetView(
		cpuDescHandle,
		clearColor,
		0,
		nullptr
	);
}
