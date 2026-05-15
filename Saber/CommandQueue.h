/**
 * @file CommandQueue.h
 * @brief Manages a D3D12 command queue together with command-list pooling,
 *        deferred execution, and fence-based synchronisation.
 */
#pragma once

#include "Headers.h"

#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "EnumFence.h"
#include "LockFreeQueue.h"

class CommandList;
class Device;
template <EnumConcept Enum>
class EnumFence;
class Fence;
class IncrementFence;

/**
 * @brief Wraps an @c ID3D12CommandQueue and owns a pool of command allocators and lists.
 *
 * Provides three kinds of submission:
 *  - Immediate: execute a command list synchronously on the calling thread.
 *  - Deferred: push a closed list to a priority set for ordered batch execution.
 *  - Execution task: drain a priority set in a worker-thread context.
 *
 * All fence operations (Signal/CpuWait/GpuWait/Flush) are overloaded for
 * @ref Fence, @ref IncrementFence, and @ref EnumFence.
 */
class CommandQueue {
    std::wstring m_name;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue{};

    std::shared_ptr<IncrementFence> m_pIncFence{};

    /** @brief Entry in the allocator pool that tracks which fence value it can be reused after. */
    struct CommandAllocatorEntry {
        uint64_t fenceValue{};
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};
    };
    ArrayLockFreeQueue<CommandAllocatorEntry> m_commandAllocatorQueue{};
    ArrayLockFreeQueue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_commandListQueue{};

    /** @brief A mutex-protected set of command lists waiting at the same priority level. */
    struct PrioritySet {
        std::mutex mutex{};
        std::unordered_multiset<std::shared_ptr<CommandList>> pCommandLists{};
    };
    std::mutex m_commandListsSetsMutex{};
    std::vector<std::unique_ptr<PrioritySet>> m_commandListSets{};

public:
    CommandQueue() = delete;
    CommandQueue(CommandQueue&&) = delete;
    CommandQueue(const CommandQueue&) = delete;

    /**
     * @brief Creates the D3D12 command queue and its internal increment fence.
     * @param name    Debug name.
     * @param pDevice Device used to create the queue and fence.
     * @param type    Queue type (Direct, Compute, or Copy).
     */
    CommandQueue(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        D3D12_COMMAND_LIST_TYPE type
    );
    ~CommandQueue();

    /** @brief Returns the underlying @c ID3D12CommandQueue COM pointer. */
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

    /** @brief Returns the command-list type this queue was created with. */
    D3D12_COMMAND_LIST_TYPE GetCommandListType() const;

    /**
     * @brief Retrieves or creates an open command list from the pool.
     * @param pDevice Device used to create new allocators or lists.
     * @return Open (already recording) command list.
     */
    std::shared_ptr<CommandList> GetCommandList(
        std::shared_ptr<Device> pDevice
    );

    /**
     * @brief Creates a deferred command list that will be ordered by @p priority.
     *
     * The returned list is closed by the caller and later submitted via
     * @ref PushForExecution / @ref ExecutionTask.
     *
     * @param name             Debug name.
     * @param pDevice          Device.
     * @param priority         Submission priority (lower index = earlier execution).
     * @param beforeExecuteTask Callback invoked just before GPU submission.
     * @param afterExecuteTask  Callback invoked after GPU execution completes.
     * @return Closed, deferred command list.
     */
    std::shared_ptr<CommandList> GetDeferredCommandList(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        size_t priority,
        std::function<void()> beforeExecuteTask = []{},
        std::function<void()> afterExecuteTask = []{}
    );

    /**
     * @brief Submits a closed command list to the GPU immediately.
     * @param commandList Closed command list to execute.
     * @return Fence value that will be signalled when execution completes.
     */
    uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);

    /**
     * @brief Executes a command list and blocks the CPU until it completes.
     * @param commandList Command list to execute.
     */
    void ExecuteCommandListImmediately(std::shared_ptr<CommandList> commandList);

    /**
     * @brief Moves a closed list into the appropriate priority set for deferred execution.
     * @param pCommandList Closed command list.
     */
    void PushForExecution(std::shared_ptr<CommandList> pCommandList);

    /**
     * @brief Drains the priority sets and executes all ready command lists.
     * @param waitFenceValue Fence value to wait on before executing.
     * @return Fence value signalled after the last submitted list.
     */
    uint64_t ExecutionTask(uint64_t waitFenceValue);

    // ---------------------------------------------------------------- Fence

    /** @brief Signals @p pFence with @p fenceValue on this queue. */
    void Signal(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);
    /** @brief CPU-waits until @p pFence reaches @p fenceValue. */
    void CpuWait(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);
    /** @brief Inserts a GPU-side wait on @p pFence before subsequent queue work. */
    void GpuWait(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);
    /** @brief Signals and CPU-waits until @p pFence reaches @p fenceValue. */
    void Flush(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);

    // ---------------------------------------------------------- IncrementFence

    /** @brief Increments and signals the increment fence; returns the new fence value. */
    uint64_t Signal(std::shared_ptr<IncrementFence>& pFence);
    /** @brief CPU-waits until the increment fence reaches @p fenceValue. */
    void CpuWait(std::shared_ptr<IncrementFence>& pFence, uint64_t fenceValue);
    /** @brief GPU-waits until the increment fence reaches @p fenceValue. */
    void GpuWait(std::shared_ptr<IncrementFence>& pFence, uint64_t fenceValue);
    /** @brief Signals and flushes the increment fence. */
    void Flush(std::shared_ptr<IncrementFence>& pFence);

    // ------------------------------------------------------------- EnumFence

    /** @brief Signals @p pFence with the underlying integer value of @p fenceValue. */
    template <EnumConcept Enum>
    void Signal(std::shared_ptr<EnumFence<Enum>>& pFence, Enum fenceValue) {
        Signal(std::static_pointer_cast<Fence>(pFence), static_cast<uint64_t>(fenceValue));
    }
    /** @brief CPU-waits until the enum fence reaches @p fenceValue. */
    template <EnumConcept Enum>
    void CpuWait(std::shared_ptr<EnumFence<Enum>>& pFence, Enum fenceValue) {
        CpuWait(std::static_pointer_cast<Fence>(pFence), static_cast<uint64_t>(fenceValue));
    }
    /** @brief GPU-waits until the enum fence reaches @p fenceValue. */
    template <EnumConcept Enum>
    void GpuWait(std::shared_ptr<EnumFence<Enum>>& pFence, Enum fenceValue) {
        GpuWait(std::static_pointer_cast<Fence>(pFence), static_cast<uint64_t>(fenceValue));
    }
    /** @brief Flushes the enum fence (signals with max underlying value). */
    template <EnumConcept Enum>
    void Flush(std::shared_ptr<EnumFence<Enum>>& pFence) {
        Flush(pFence, std::numeric_limits<std::underlying_type_t<Enum>>::max());
    }

    // --------------------------------------- Queue's own increment fence

    /** @brief Signals the queue's internal increment fence and returns the new value. */
    uint64_t Signal();
    /** @brief CPU-waits until the queue's internal fence reaches @p fenceValue. */
    void CpuWait(uint64_t fenceValue);
    /** @brief GPU-waits until the queue's internal fence reaches @p fenceValue. */
    void GpuWait(uint64_t fenceValue);
    /** @brief Signals and CPU-waits until all submitted work on this queue completes. */
    void Flush();
    /** @brief Returns @c true if the queue's internal fence has passed @p fenceValue. */
    bool IsFenceComplete(uint64_t fenceValue);

private:
    /**
     * @brief Creates and returns a new @c ID3D12CommandQueue of the specified type.
     * @param pDevice Device used to create the queue.
     * @param type    Queue type.
     * @return Newly created command queue.
     */
    static Microsoft::WRL::ComPtr<ID3D12CommandQueue> CreateCommandQueue(
        std::shared_ptr<Device> pDevice,
        D3D12_COMMAND_LIST_TYPE type
    );

    /**
     * @brief Returns a @c ID3D12GraphicsCommandList2 from the pool (or creates one).
     * @param pDevice Device used to allocate new lists.
     * @return Open command list ready for recording.
     */
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList(
        std::shared_ptr<Device> pDevice
    );

    /**
     * @brief Submits a raw D3D12 command list to the GPU and returns a fence value.
     * @param pD3D12CommandList Closed D3D12 command list.
     * @return Fence value that will be signalled on completion.
     */
    uint64_t ExecuteD3D12CommandList(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pD3D12CommandList
    );

    /**
     * @brief Allocates a new @c ID3D12CommandAllocator for this queue's type.
     * @param pDevice Device used for creation.
     * @return Newly created allocator.
     */
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
        std::shared_ptr<Device> pDevice
    ) const;

    /**
     * @brief Creates a new @c ID3D12GraphicsCommandList2 backed by @p pAllocator.
     * @param pDevice    Device.
     * @param pAllocator Allocator to use.
     * @return Newly created, open command list.
     */
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(
        std::shared_ptr<Device> pDevice,
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator
    ) const;
};
