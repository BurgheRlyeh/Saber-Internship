#include "IncrementFence.h"

#include "CommandQueue.h"

uint64_t Signal(CommandQueue* pCommandQueue, IncrementFence* pIncFence) {
    uint64_t fenceValue{ ++pIncFence->m_fenceValue };
    ::Signal(pCommandQueue, static_cast<Fence*>(pIncFence), fenceValue);
    return fenceValue;
}
void Flush(CommandQueue* pCommandQueue, IncrementFence* pIncFence) {
    CpuWait(static_cast<Fence*>(pIncFence), ::Signal(pCommandQueue, pIncFence));
}

IncrementFence::IncrementFence(
    const std::wstring& name,
    std::shared_ptr<Device> pDevice,
    D3D12_FENCE_FLAGS flags
) : Fence(name, pDevice, IncrementFenceInitValue, flags) {}

bool IncrementFence::IsCompleted(uint64_t fenceValue) {
    return Fence::GetValue() >= fenceValue;
}