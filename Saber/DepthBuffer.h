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

class DepthBuffer {
protected:
	std::wstring m_name{};

	std::shared_ptr<TextureResource> m_pDepthBuffer{};

	std::shared_ptr<DescRange> m_pDsvsRange{};
	std::shared_ptr<DescRange> m_pSrvsRange{};

public:
	DepthBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		UINT64 width,
		UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT,
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

	D3D12_RESOURCE_DESC GetDesc() const;

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

enum class DepthBufferState : uint8_t {
	InvalidState = 0,

	DepthWriting,
	HierarchicalDepthBuilding,
	DepthReading,

	FlushState = std::numeric_limits<uint8_t>::max()
};

class HiDepthBuffer : public DepthBuffer {
	static constexpr size_t HzbMaxResolution{ 4096 };

	static constexpr size_t HzbMaxMipCount{ 13 };
	static constexpr size_t HzbMidMipUavId{ 6 };

	std::shared_ptr<TextureResource> m_pHZBuffer{};

	std::shared_ptr<DescRange> m_pHzbSrvsRange{};
	std::shared_ptr<DescRange> m_pHzbUavsRange{};

	std::shared_ptr<SinglePassDownsampler> m_pSinglePassDownsampler{};

	std::shared_ptr<EnumFence<DepthBufferState>> m_pDepthBufferFence{};

public:
	HiDepthBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		UINT64 width,
		UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	void CreateHierarchicalDepthBuffer(
		std::shared_ptr<CommandList> pCommandList,
		const std::shared_ptr<DescriptorHeap>& pResDescHeap
	);

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandleWithMips() const;

	std::shared_ptr<EnumFence<DepthBufferState>> GetFence() const;
	void SignalState(std::shared_ptr<CommandList>& pCommandList, DepthBufferState state);
	void WaitState(std::shared_ptr<CommandList>& pCommandList, DepthBufferState state);

protected:
	void Recreate(
		std::shared_ptr<Device> pDevice,
		const D3D12_RESOURCE_DESC& desc
	) override;

private:
	void RecreateHiDepthBuffer(
		std::shared_ptr<Device> pDevice,
		const D3D12_RESOURCE_DESC& desc
	);

	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMips() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMidMip() const;
};
