/**
 * @file IncrementFence.h
 * @brief Declares @ref IncrementFence — a monotonically-incrementing GPU fence
 *        used to track frame completion.
 */
#pragma once

#include "Fence.h"

class CommandQueue;
class Device;
class IncrementFence;

/**
 * @brief Signals an increment fence on the given queue and returns the new fence value.
 * @param pCommandQueue Queue that issues the GPU signal command.
 * @param pFence        Fence to increment and signal.
 * @return The fence value that was signalled.
 */
uint64_t Signal(CommandQueue* pCommandQueue, IncrementFence* pFence);

/**
 * @brief Signals and then CPU-waits until the increment fence has completed.
 * @param pCommandQueue Queue used to issue the final signal.
 * @param pFence        Fence to flush.
 */
void Flush(CommandQueue* pCommandQueue, IncrementFence* pFence);

/**
 * @brief A GPU fence whose value is automatically incremented on each signal.
 *
 * Each call to @c Signal() bumps the internal counter and issues the new value
 * to the GPU, making it easy to track frame boundaries without managing raw values.
 */
class IncrementFence : public Fence {
public:
    /** @brief The value the fence is initialised with (1, so 0 always reads as "complete"). */
    static constexpr uint64_t IncrementFenceInitValue{ 1 };

    /**
     * @brief Constructs an IncrementFence starting at @ref IncrementFenceInitValue.
     * @param name   Debug name.
     * @param pDevice Device used to create the underlying D3D12 fence.
     * @param flags  Optional fence creation flags.
     */
    IncrementFence(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_NONE
    );

    /**
     * @brief Returns true if the GPU has completed work up to @p fenceValue.
     * @param fenceValue Fence value to test.
     * @return @c true if the fence has reached or exceeded @p fenceValue.
     */
    bool IsCompleted(uint64_t fenceValue);

private:
    uint64_t m_fenceValue{ IncrementFenceInitValue }; /**< @brief Next value to signal. */

    friend uint64_t Signal(CommandQueue*, IncrementFence*);
};
