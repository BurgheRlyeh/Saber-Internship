#pragma once

#include "Headers.h"
#include "EnumFence.h"

class CommandList;
class DescRange;
class DescriptorHeap;
class Device;
class DeviceContext;
class SinglePassDownsampler;
class TextureResource;

enum class DepthStencilSubresources : uint32_t {
	Depth = 0,
	Stencil,

	Count
};

enum class DepthBufferType : uint8_t {
	DepthOnly = 0,
	DepthStencil,

	Count
};

enum class DepthBufferState : uint8_t {
	InvalidState = 0,

	DepthWriting,
	HierarchicalDepthBuilding,
	DepthReading,

	FlushState = std::numeric_limits<uint8_t>::max()
};

class DepthBuffer {
protected:
	std::wstring m_name{};

	DepthBufferType m_type{ DepthBufferType::DepthOnly };

	std::shared_ptr<TextureResource> m_pDepthTarget{};

	std::shared_ptr<DescRange> m_pDsvsRange{};
	std::shared_ptr<DescRange> m_pSrvsRange{};

	DepthBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		UINT64 width,
		UINT height,
		DXGI_FORMAT format,
		DepthBufferType type,
		D3D12_RESOURCE_FLAGS flags,
		size_t extraSrvCount
	);

public:
	DepthBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		UINT64 width,
		UINT height,
		DXGI_FORMAT format,
		DepthBufferType type = DepthBufferType::DepthOnly,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);
	virtual ~DepthBuffer() = default;

	void Resize(
		std::shared_ptr<Device> pDevice,
		UINT64 width,
		UINT height
	);

	void Clear(std::shared_ptr<CommandList> pCommandList);

	std::shared_ptr<TextureResource> GetTexture() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandle(
		DepthStencilSubresources subresource = DepthStencilSubresources::Depth
	) const;

protected:
	virtual void Recreate(
		std::shared_ptr<Device> pDevice,
		const D3D12_RESOURCE_DESC& desc
	);
};

class HiDepthBuffer : public DepthBuffer {
	std::shared_ptr<TextureResource> m_pHZBuffer{};

	size_t m_hzbSrvId{};
	std::shared_ptr<DescRange> m_pUavsRange{};

	static const size_t HzbWithMips{ 13 };
	static const size_t HzbMidMip{ 6 };

	std::shared_ptr<SinglePassDownsampler> m_pSinglePassDownsampler{};

	std::shared_ptr<EnumFence<DepthBufferState>> m_pDepthBufferFence{};

public:
	HiDepthBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		UINT64 width,
		UINT height,
		DXGI_FORMAT format,
		DepthBufferType type = DepthBufferType::DepthOnly,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	void CreateHierarchicalDepthBuffer(
		std::shared_ptr<CommandList> pCommandList,
		std::shared_ptr<DescriptorHeap> pResDescHeapManager
	);

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandleWithMips() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMidMip() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMips() const;

	std::shared_ptr<EnumFence<DepthBufferState>> GetFence() const;

protected:
	void Recreate(
		std::shared_ptr<Device> pDevice,
		const D3D12_RESOURCE_DESC& desc
	) override;

	void RecreateHiDepthBuffer(
		std::shared_ptr<Device> pDevice,
		const D3D12_RESOURCE_DESC& desc
	);
};
