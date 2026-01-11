#pragma once

#include "Headers.h"

#include <array>

#include "Atlas.h"
#include "DynamicUploadRingBuffer.h"
#include "FencedQueue.h"

class CommandQueue;
class DescriptorHeapManager;
class Device;
template <typename T>
class FrameDataBuffer;
class GPUResource;
class MaterialManager;
class Mesh;
class PSOLibrary;

struct RootSignatureResource;
struct ShaderResource;

class DeviceContext {
	std::wstring m_name{};

	std::shared_ptr<Device> m_pDevice{};

	// Command Queues
	std::shared_ptr<CommandQueue> m_pCommandQueueDirect{};
	std::shared_ptr<CommandQueue> m_pCommandQueueCompute{};
	std::shared_ptr<CommandQueue> m_pCommandQueueCopy{};

	std::array<std::shared_ptr<DescriptorHeapManager>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_pDescHeaps{};

	// Atlases
	std::shared_ptr<Atlas<RootSignatureResource>> m_pRootSignatureAtlas{};
	std::shared_ptr<Atlas<ShaderResource>> m_pShaderAtlas{};
	std::shared_ptr<PSOLibrary> m_pPSOLibrary{};

	std::shared_ptr<Atlas<Mesh>> m_pMeshAtlas{};
	std::shared_ptr<MaterialManager> m_pMaterialManager{};

	// Ring Buffers
	std::array<std::shared_ptr<DynamicUploadHeap>, static_cast<size_t>(RingBufferType::Count)> m_pRingBuffers{};
	std::shared_ptr<FrameDataBuffer<std::shared_ptr<GPUResource>>> m_pFrameDataBuffer{};

public:
	struct DescHeapArgs {
		size_t size{};
		D3D12_DESCRIPTOR_HEAP_FLAGS flags{};
	};
	DeviceContext(Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter);
	void InitializeContext(
		const std::array<DescHeapArgs, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& descHeapArgs
	);

	std::shared_ptr<Device> GetDevice() const {
		return m_pDevice;
	}

	std::shared_ptr<CommandQueue> GetCommandQueue(
		const D3D12_COMMAND_LIST_TYPE& type = D3D12_COMMAND_LIST_TYPE_DIRECT
	) const {
		switch (type) {
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			return m_pCommandQueueDirect;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			return m_pCommandQueueCompute;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			return m_pCommandQueueCopy;
		default:
			throw std::runtime_error("Unsupported CommandQueue type");
		}
	}

	std::shared_ptr<DescriptorHeapManager> GetDescriptorHeap(
		const D3D12_DESCRIPTOR_HEAP_TYPE& type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	) const {
		assert(0 <= type && type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
		return m_pDescHeaps[type];
	}

	std::shared_ptr<Atlas<RootSignatureResource>> GetRootSignatureAtlas() const {
		return m_pRootSignatureAtlas;
	}
	std::shared_ptr<Atlas<ShaderResource>> GetShaderAtlas() const {
		return m_pShaderAtlas;
	}
	std::shared_ptr<PSOLibrary> GetPSOLibrary() const {
		return m_pPSOLibrary;
	}

	std::shared_ptr<Atlas<Mesh>> GetMeshAtlas() const {
		return m_pMeshAtlas;
	}

	void SetMaterialManager(std::shared_ptr<MaterialManager> pMaterialManager) {
		m_pMaterialManager = pMaterialManager;
	}
	std::shared_ptr<MaterialManager> GetMaterialManager() const {
		return m_pMaterialManager;
	}

	std::shared_ptr<DynamicUploadHeap> GetRingBuffer(
		const RingBufferType& type = RingBufferType::CPU
	) const {
		return m_pRingBuffers[static_cast<size_t>(type)];
	}

	void AddIntermediate(std::shared_ptr<GPUResource> pResource) {
		m_pFrameDataBuffer->Add(pResource);
	}

	void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
		for (auto& pRingBuffer : m_pRingBuffers) {
			pRingBuffer->FinishFrame(fenceValue, lastCompletedFenceValue);
		}
		m_pFrameDataBuffer->FinishFrame(fenceValue, lastCompletedFenceValue);
	}

private:
	static void SetInfoQueueFilter(Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice);
};
