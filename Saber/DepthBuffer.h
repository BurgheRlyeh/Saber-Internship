#pragma once

#include "Headers.h"
#include "EnumFence.h"

class CommandList;
class DescRange;
class Device;
class DeviceContext;
class SinglePassDownsampler;
class TextureResource;

enum class DepthBufferState : uint8_t {
	InvalidState = 0,

	DepthWriting,
	HierarchicalDepthBuilding,
	DepthReading,

	FlushState = std::numeric_limits<uint8_t>::max()
};

class DepthBuffer {
	static inline D3D12_RESOURCE_DESC m_depthBufferDesc{
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	};
	static inline D3D12_CLEAR_VALUE m_clearValue{
		.Format{ DXGI_FORMAT_D32_FLOAT },
		.DepthStencil{ 0.0f, 0 }
	};

	std::wstring m_name{};

	std::shared_ptr<TextureResource> m_pDepthBuffer{};
	std::shared_ptr<TextureResource> m_pHZBuffer{};

	std::shared_ptr<DescRange> m_pDsvsRange{};
	std::shared_ptr<DescRange> m_pSrvsRange{};
	size_t m_depthSrvId{};
	size_t m_hzbSrvId{};
	std::shared_ptr<DescRange> m_pUavsRange{};

	std::shared_ptr<SinglePassDownsampler> m_pSinglePassDownsampler{};

	static const size_t m_hzbSize{ 12 };
	static const size_t m_hzbMidMipId{ 5 };

	size_t m_width{};
	size_t m_height{};
	std::shared_ptr<EnumFence<DepthBufferState>> m_pDepthBufferFence{};

public:
	DepthBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		UINT64 width,
		UINT height,
		std::shared_ptr<SinglePassDownsampler> pSPD = nullptr
	);

	void Resize(
		std::shared_ptr<Device> pDevice,
		UINT64 width,
		UINT height
	);
	bool ResizeHZB(
		std::shared_ptr<Device> pDevice,
		UINT64 width,
		UINT height
	);

	void Clear(std::shared_ptr<CommandList> pCommandList);

	void SetSinglePassDownsampler(
		std::shared_ptr<SinglePassDownsampler> pSPD,
		std::shared_ptr<Device> pDevice,
		UINT64 width,
		UINT height
	);

	void CreateHierarchicalDepthBuffer(
		std::shared_ptr<CommandList> pCommandList,
		Microsoft::WRL::ComPtr<D3D12DescriptorHeap> pDescHeap
	);

	std::shared_ptr<TextureResource> GetTexture() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpuDescHandle() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandleWithMips() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMidMip() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMips() const;

	std::shared_ptr<EnumFence<DepthBufferState>> GetFence() const;
	void SignalState(std::shared_ptr<CommandList>& pCommandList, DepthBufferState state);
	void WaitState(std::shared_ptr<CommandList>& pCommandList, DepthBufferState state);
};
