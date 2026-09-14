#pragma once

#include "Headers.h"

#include <mutex>

#include "CommandList.h"
#include "FencedQueue.h"
#include "GPUResource.h"

class Device;

// Pulls a small block of numbers back from the GPU without ever stalling it: every
// frame copies into a slot of its own, and a slot is only mapped once the fence of
// its frame has passed. What comes out is therefore as old as the number of frames
// in flight, which is what statistics can live with
template <typename T>
class ReadbackBuffer : public FrameFencedQueue<size_t> {
	std::shared_ptr<GPUResource> m_pResource{};

	size_t m_slotCount{};
	size_t m_currSlot{};

	T m_latest{};
	mutable std::mutex m_latestMutex{};

public:
	ReadbackBuffer(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		size_t slotCount
	) : FrameFencedQueue<size_t>(0),	// starts empty, a prefilled queue never pops
		m_slotCount(slotCount)
	{
		m_pResource = std::make_shared<GPUResource>(
			name,
			pDevice,
			GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_READBACK },
			GPUResource::ResourceDesc{
				CD3DX12_RESOURCE_DESC::Buffer(slotCount * sizeof(T)),
				// A readback resource starts in copy destination and never leaves it
				D3D12_RESOURCE_STATE_COPY_DEST
			}
		);
	}

	// Records the copy into the slot the frame being recorded owns
	void Copy(
		std::shared_ptr<CommandList> pCommandList,
		const std::shared_ptr<GPUResource>& pSource,
		uint64_t sourceOffset = 0
	) {
		pSource->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		pCommandList->GetD3D12CommandList()->CopyBufferRegion(
			m_pResource->GetD3D12Resource().Get(),
			m_currSlot * sizeof(T),
			pSource->GetD3D12Resource().Get(),
			sourceOffset,
			sizeof(T)
		);
	}

	T GetLatest() const {
		std::scoped_lock<std::mutex> lock(m_latestMutex);
		return m_latest;
	}

protected:
	size_t ProduceForPush() override {
		size_t slot{ m_currSlot };
		m_currSlot = (m_currSlot + 1) % m_slotCount;
		return slot;
	}

	void BeforePop(const size_t& slot) override {
		const D3D12_RANGE readRange{
			slot * sizeof(T),
			(slot + 1) * sizeof(T)
		};

		T* pData{};
		ThrowIfFailed(m_pResource->GetD3D12Resource()->Map(
			0, &readRange, reinterpret_cast<void**>(&pData)
		));

		{
			std::scoped_lock<std::mutex> lock(m_latestMutex);
			m_latest = pData[slot];
		}

		// Nothing was written from the cpu side
		m_pResource->GetD3D12Resource()->Unmap(0, &CD3DX12_RANGE(0, 0));
	}
};
