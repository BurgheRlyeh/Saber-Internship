#include "Fence.h"

#include "CommandQueue.h"
#include "Device.h"

void Signal(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue) {
    pCommandQueue->GetD3D12CommandQueue()->Signal(pFence->GetD3D12Fence().Get(), fenceValue);
}
void CpuWait(Fence* pFence, uint64_t fenceValue) {
    if (pFence->GetValue() != fenceValue) {
        ThrowIfFailed(pFence->GetD3D12Fence()->SetEventOnCompletion(fenceValue, pFence->GetEvent()));
        ::WaitForSingleObject(pFence->GetEvent(), DWORD_MAX);
    }
}
void GpuWait(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue) {
    ThrowIfFailed(pCommandQueue->GetD3D12CommandQueue()->Wait(pFence->GetD3D12Fence().Get(), fenceValue));
}
void Flush(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue) {
    Signal(pCommandQueue, pFence, fenceValue);
    CpuWait(pFence, fenceValue);
}

Fence::Fence(
    const std::wstring& name,
    std::shared_ptr<Device> pDevice,
    uint64_t fenceInitValue,
    D3D12_FENCE_FLAGS flags
) : m_name(name) {
    ThrowIfFailed(pDevice->GetD3D12Device()->CreateFence(
        fenceInitValue,
        flags,
        IID_PPV_ARGS(&m_pFence)
    ));
    m_pFence->SetName((m_name + L"/Fence").c_str());

    m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(m_fenceEvent && "Failed to create fence event handle.");
}

Fence::~Fence() {
    ::CloseHandle(m_fenceEvent);
}

Microsoft::WRL::ComPtr<D3D12Fence> Fence::GetD3D12Fence() const {
    return m_pFence;
}

HANDLE Fence::GetEvent() const {
    return m_fenceEvent;
}

uint64_t Fence::GetValue() const {
    return m_pFence->GetCompletedValue();
}
