#pragma once

#include "Headers.h"

#include <array>
#include <stdexcept>

#include "Atlas.h"
#include "CommandListTypes.h"
#include "DynamicUploadRingBuffer.h"
#include "DescriptorHeapManager.h"
#include "FencedQueue.h"

class CommandQueue;
class CommandListManager;
class DescriptorHeap;
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

	std::shared_ptr<CommandListManager> m_pCommandListMgr{};

	std::shared_ptr<DescriptorHeapManager> m_pDescHeapManager{};

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
	DeviceContext(
		Microsoft::WRL::ComPtr<DXGIAdapter> pAdapter,
		const std::array<DescriptorHeapManager::DescHeapArgs, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& descHeapArgs
	);
	~DeviceContext();

	std::shared_ptr<Device> GetDevice() const {
		return m_pDevice;
	}

	// CommandListManager
	std::shared_ptr<CommandListManager> GetCommandListManager() const {
		return m_pCommandListMgr;
	}
	std::shared_ptr<CommandQueue> GetCommandQueue(CommandQueueType type = CommandQueueType::Direct) const;

	std::shared_ptr<DescriptorHeap> GetDescriptorHeap(DescRangeType descRangeType) const {
		return m_pDescHeapManager->GetDescHeap(descRangeType);
	}

	template <std::derived_from<DescRange> DescRangeImpl = StackDescRange>
	std::shared_ptr<DescRange> AllocateDescRange(const std::wstring& basename, DescRangeType descRangeType, size_t size) {
		return m_pDescHeapManager->AllocateRange<DescRangeImpl>(basename, descRangeType, size);
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
	static void SetInfoQueueFilter(Microsoft::WRL::ComPtr<D3D12Device>& pDevice);
};
