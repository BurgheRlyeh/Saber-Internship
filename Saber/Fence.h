/**
 * @file Fence.h
 * @brief Declares the base @ref Fence class and free-function helpers for
 *        GPU/CPU synchronization via D3D12 fences.
 */
#pragma once

#include "Headers.h"

#include <limits>
#include <type_traits>

class CommandQueue;
class Device;
class Fence;

/**
 * @brief Signals a fence on the given command queue with the specified value.
 * @param pCommandQueue Queue that issues the signal command.
 * @param pFence        Fence to signal.
 * @param fenceValue    Value to signal on the fence.
 */
void Signal(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue);

/**
 * @brief Blocks the CPU thread until the fence reaches or exceeds @p fenceValue.
 * @param pFence     Fence to wait on.
 * @param fenceValue Target fence value.
 */
void CpuWait(Fence* pFence, uint64_t fenceValue);

/**
 * @brief Inserts a GPU-side wait on the fence before subsequent queue work.
 * @param pCommandQueue Queue that will stall until the fence is signalled.
 * @param pFence        Fence to wait on.
 * @param fenceValue    Target fence value.
 */
void GpuWait(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue);

/**
 * @brief Signals the fence and then blocks the CPU until that signal completes.
 * @param pCommandQueue Queue used to issue the signal.
 * @param pFence        Fence to flush.
 * @param fenceValue    Value to signal and wait for.
 */
void Flush(CommandQueue* pCommandQueue, Fence* pFence, uint64_t fenceValue);

/**
 * @brief Thin wrapper around an @c ID3D12Fence with an associated CPU event handle.
 */
class Fence {
protected:
    std::wstring m_name{};                                /**< @brief Debug name. */
    Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence{};      /**< @brief Underlying D3D12 fence. */
    HANDLE m_fenceEvent{};                                /**< @brief CPU event signalled when the fence value is reached. */

public:
    /**
     * @brief Creates a D3D12 fence and an associated Win32 event.
     * @param name           Debug name for this fence.
     * @param pDevice        Device used to create the D3D12 fence.
     * @param fenceInitValue Initial fence value (default 0).
     * @param flags          Optional fence creation flags.
     */
    Fence(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        uint64_t fenceInitValue = 0,
        D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_NONE
    );

    virtual ~Fence();

    /** @brief Returns the underlying @c ID3D12Fence COM object. */
    Microsoft::WRL::ComPtr<ID3D12Fence> GetD3D12Fence() const;

    /** @brief Returns the Win32 event handle used for CPU-side waiting. */
    HANDLE GetEvent() const;

    /** @brief Returns the current completed value of the fence. */
    uint64_t GetValue() const;
};
