#pragma once

#include "Headers.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "CommandListTypes.h"
#include "EnumHelpers.h"
#include "IncrementFence.h"

class CommandList;
class CommandQueue;
class Device;

class CommandListManager {
    static constexpr size_t MaxPriority{ 10 };
    static constexpr std::chrono::seconds kReadyTimeout{ 5 };

    std::shared_ptr<Device> m_pDevice{};

    std::array<std::shared_ptr<CommandQueue>, static_cast<size_t>(CommandQueueType::Count)> m_pCommandQueues{};

    // Deferred lists of the same priority
    struct DeferredLists {
        std::mutex mutex{};
        std::condition_variable readyCv{};
        std::vector<std::shared_ptr<CommandList>> pCommandLists{};
        size_t unreadyCount{};
    };
    std::array<DeferredLists, MaxPriority> m_deferredLists{};

public:
    CommandListManager(std::shared_ptr<Device> pDevice);
    ~CommandListManager();

    std::shared_ptr<CommandQueue> GetCommandQueue(CommandQueueType type) const {
        return m_pCommandQueues[static_cast<size_t>(type)];
    }

    std::shared_ptr<CommandList> GetCommandList(
        CommandListType type = CommandListType::Direct
    );

    std::shared_ptr<CommandList> GetDeferredCommandList(
        const std::wstring& name,
        CommandListType type,
        size_t priority,
        std::function<void()> beforeExecuteTask = []{},
        std::function<void()> afterExecuteTask = []{}
    );

    uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
    void ExecuteCommandListImmediately(std::shared_ptr<CommandList> commandList);

    void PushForExecution(std::shared_ptr<CommandList> pCommandList);

    void ExecutionTask(FrameFenceValues& waitFenceValues);

    // TODO: the batches still live one queue at a time, which is why this has
    // to fan out. A batch does not need its queue though, only the fence it was
    // stamped with, so all of them could sit in one pool keyed by
    // { IncrementFence, value } owned here. That would give: one place holding
    // every deferred resource, so a staging memory counter or a debug view is a
    // few lines instead of a sum over queues; CommandQueue no longer storing
    // resources at all, dropping its dependency on GPUResource; no dependency
    // cycle, since the pool would know IncrementFence and not CommandQueue; and
    // recording still lock free, with one lock per submission instead of one
    // per queue. Deferred because it is a bigger change than moving this call.
    void ReleaseCompletedResources();

    void Flush();
};
