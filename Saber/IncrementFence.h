#pragma once

#include "Fence.h"

class CommandQueue;
class Device;
class IncrementFence;

uint64_t Signal(CommandQueue* pCommandQueue, IncrementFence* pFence);
void Flush(CommandQueue* pCommandQueue, IncrementFence* pFence);

class IncrementFence : public Fence {
public:
    static constexpr uint64_t IncrementFenceInitValue{ 1 };

    IncrementFence(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_NONE
    );

    bool IsCompleted(uint64_t fenceValue);

private:
    uint64_t m_fenceValue{ IncrementFenceInitValue };

    friend uint64_t Signal(CommandQueue*, IncrementFence*);
};
