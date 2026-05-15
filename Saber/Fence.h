#pragma once

#include "Headers.h"

#include <limits>
#include <type_traits>

class CommandQueue;
class Device;
class Fence;

void Signal(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue);
void CpuWait(Fence* pFence, uint64_t fenceValue);
void GpuWait(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue);
void Flush(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue);

class Fence {
protected:
    std::wstring m_name{};

    Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence{};
    HANDLE m_fenceEvent{};

public:
    Fence(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        uint64_t fenceInitValue = 0,
        D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_NONE
    );
    virtual ~Fence();

    Microsoft::WRL::ComPtr<ID3D12Fence> GetD3D12Fence() const;
    HANDLE GetEvent() const;

    uint64_t GetValue() const;
};
